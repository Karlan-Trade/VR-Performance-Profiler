using System.Diagnostics;
using System.Text.Json;
using System.Text.Json.Serialization;
using LibreHardwareMonitor.Hardware;

var options = BridgeOptions.Parse(args);
Directory.CreateDirectory(Path.GetDirectoryName(options.OutputPath)!);

using var bridge = new SensorBridge(options.OutputPath);
bridge.Open();

if (options.Once)
{
    if (options.DumpReport)
    {
        bridge.WriteReport();
    }
    else if (options.DumpRaw)
    {
        bridge.WriteRawSnapshot();
    }
    else
    {
        bridge.WriteSnapshot();
    }

    return 0;
}

using var quitEvent = new ManualResetEventSlim(false);
Console.CancelKeyPress += (_, eventArgs) =>
{
    eventArgs.Cancel = true;
    quitEvent.Set();
};

while (!quitEvent.IsSet)
{
    bridge.WriteSnapshot();
    quitEvent.Wait(options.Interval);
}

return 0;

internal sealed class SensorBridge : IDisposable
{
    private static readonly JsonSerializerOptions JsonOptions = new()
    {
        WriteIndented = true,
        DefaultIgnoreCondition = JsonIgnoreCondition.WhenWritingNull
    };

    private readonly string outputPath;
    private readonly Computer computer = new()
    {
        IsCpuEnabled = true,
        IsGpuEnabled = true,
        IsMemoryEnabled = true,
        IsMotherboardEnabled = true,
        IsStorageEnabled = true,
        IsControllerEnabled = true
    };

    public SensorBridge(string outputPath)
    {
        this.outputPath = outputPath;
    }

    public void Open()
    {
        computer.Open();
    }

    public void WriteSnapshot()
    {
        var readings = new Dictionary<string, SensorReadingDto>();
        foreach (var hardware in computer.Hardware)
        {
            UpdateHardware(hardware);
            AddReadings(hardware, readings);
        }

        var snapshot = new SensorSnapshotDto
        {
            Source = "LibreHardwareMonitor",
            TimestampUtc = DateTimeOffset.UtcNow,
            ProcessId = Environment.ProcessId,
            Readings = readings.Values.ToArray()
        };

        WriteJsonSnapshot(snapshot);
    }

    public void WriteRawSnapshot()
    {
        var hardwareItems = new List<RawHardwareDto>();
        foreach (var hardware in computer.Hardware)
        {
            UpdateHardware(hardware);
            AddRawHardware(hardware, hardwareItems);
        }

        var snapshot = new RawSensorSnapshotDto
        {
            Source = "LibreHardwareMonitor",
            TimestampUtc = DateTimeOffset.UtcNow,
            ProcessId = Environment.ProcessId,
            Hardware = hardwareItems
        };

        WriteJsonSnapshot(snapshot);
    }

    public void WriteReport()
    {
        File.WriteAllText(outputPath, computer.GetReport());
    }

    private void WriteJsonSnapshot<TSnapshot>(TSnapshot snapshot)
    {
        var tempPath = outputPath + ".tmp";
        File.WriteAllText(tempPath, JsonSerializer.Serialize(snapshot, JsonOptions));
        File.Copy(tempPath, outputPath, overwrite: true);
        File.Delete(tempPath);
    }

    private static void UpdateHardware(IHardware hardware)
    {
        hardware.Update();
        foreach (var subHardware in hardware.SubHardware)
        {
            UpdateHardware(subHardware);
        }
    }

    private static void AddReadings(IHardware hardware, Dictionary<string, SensorReadingDto> readings)
    {
        foreach (var sensor in hardware.Sensors)
        {
            var reading = ConvertSensor(hardware, sensor);
            if (reading is not null)
            {
                AddBestReading(readings, reading);
            }
        }

        foreach (var subHardware in hardware.SubHardware)
        {
            AddReadings(subHardware, readings);
        }
    }

    private static void AddBestReading(
        Dictionary<string, SensorReadingDto> readings,
        SensorReadingDto reading)
    {
        if (!IsUsableReading(reading))
        {
            return;
        }

        if (!readings.TryGetValue(reading.Category, out var existing) ||
            GetPreferenceScore(reading) > GetPreferenceScore(existing))
        {
            readings[reading.Category] = reading;
        }
    }

    private static void AddRawHardware(IHardware hardware, List<RawHardwareDto> hardwareItems)
    {
        hardwareItems.Add(new RawHardwareDto
        {
            Name = hardware.Name,
            HardwareType = hardware.HardwareType.ToString(),
            Identifier = hardware.Identifier.ToString(),
            Sensors = hardware.Sensors
                .Select(sensor => new RawSensorReadingDto
                {
                    Name = sensor.Name,
                    SensorType = sensor.SensorType.ToString(),
                    Identifier = sensor.Identifier.ToString(),
                    Value = sensor.Value,
                    Min = sensor.Min,
                    Max = sensor.Max
                })
                .ToArray()
        });

        foreach (var subHardware in hardware.SubHardware)
        {
            AddRawHardware(subHardware, hardwareItems);
        }
    }

    private static SensorReadingDto? ConvertSensor(IHardware hardware, ISensor sensor)
    {
        if (!sensor.Value.HasValue)
        {
            return null;
        }

        var category = GetCategory(hardware, sensor);
        if (category is null)
        {
            return null;
        }

        return new SensorReadingDto
        {
            Category = category,
            Label = GetLabel(hardware, sensor),
            Value = Math.Round(sensor.Value.Value, 2),
            Unit = GetUnit(sensor.SensorType)
        };
    }

    private static bool IsUsableReading(SensorReadingDto reading)
    {
        if (!double.IsFinite(reading.Value))
        {
            return false;
        }

        return reading.Category switch
        {
            "cpu_temp" or "gpu_temp" or "cpu_clock" or "gpu_clock" or "power" or "voltage" => reading.Value > 0,
            "gpu_memory" => reading.Value >= 0,
            _ => true
        };
    }

    private static int GetPreferenceScore(SensorReadingDto reading)
    {
        return reading.Category switch
        {
            "ram_usage" => GetRamPreferenceScore(reading.Label),
            "gpu_memory" => GetGpuMemoryPreferenceScore(reading.Label),
            "power" => Contains(reading.Label, "Package") || Contains(reading.Label, "Total") ? 2 : 1,
            _ => 1
        };
    }

    private static int GetRamPreferenceScore(string label)
    {
        if (Contains(label, "Virtual") || Contains(label, "虚拟"))
        {
            return 0;
        }

        if (Contains(label, "Physical") || Contains(label, "Total") || Contains(label, "物理"))
        {
            return 2;
        }

        return 1;
    }

    private static int GetGpuMemoryPreferenceScore(string label)
    {
        if (Contains(label, "Used"))
        {
            return 3;
        }

        if (Contains(label, "Total"))
        {
            return 1;
        }

        if (Contains(label, "Free"))
        {
            return 0;
        }

        return 2;
    }

    private static string? GetCategory(IHardware hardware, ISensor sensor)
    {
        return sensor.SensorType switch
        {
            SensorType.Temperature when IsCpu(hardware) => "cpu_temp",
            SensorType.Temperature when IsGpu(hardware) => "gpu_temp",
            SensorType.Load when IsCpu(hardware) && IsTotal(sensor) => "cpu_load",
            SensorType.Load when IsGpu(hardware) && IsCore(sensor) => "gpu_load",
            SensorType.Clock when IsCpu(hardware) && IsCore(sensor) => "cpu_clock",
            SensorType.Clock when IsGpu(hardware) && IsCore(sensor) => "gpu_clock",
            SensorType.SmallData when IsGpu(hardware) && IsMemory(sensor) => "gpu_memory",
            SensorType.Fan when IsGpu(hardware) => "gpu_fan",
            SensorType.Power when IsCpu(hardware) || IsGpu(hardware) => "power",
            SensorType.Voltage when IsCpu(hardware) || IsGpu(hardware) => "voltage",
            SensorType.Load when hardware.HardwareType == HardwareType.Memory => "ram_usage",
            _ => null
        };
    }

    private static string GetLabel(IHardware hardware, ISensor sensor)
    {
        return string.IsNullOrWhiteSpace(sensor.Name)
            ? hardware.Name
            : $"{hardware.Name} {sensor.Name}";
    }

    private static string GetUnit(SensorType sensorType)
    {
        return sensorType switch
        {
            SensorType.Temperature => "C",
            SensorType.Load => "%",
            SensorType.Clock => "MHz",
            SensorType.SmallData => "MB",
            SensorType.Fan => "RPM",
            SensorType.Power => "W",
            SensorType.Voltage => "V",
            _ => string.Empty
        };
    }

    private static bool IsCpu(IHardware hardware)
    {
        return hardware.HardwareType == HardwareType.Cpu;
    }

    private static bool IsGpu(IHardware hardware)
    {
        return hardware.HardwareType is HardwareType.GpuAmd
            or HardwareType.GpuNvidia
            or HardwareType.GpuIntel;
    }

    private static bool IsTotal(ISensor sensor)
    {
        return sensor.Name.Contains("Total", StringComparison.OrdinalIgnoreCase)
            || sensor.Name.Contains("Package", StringComparison.OrdinalIgnoreCase);
    }

    private static bool IsCore(ISensor sensor)
    {
        return sensor.Name.Contains("Core", StringComparison.OrdinalIgnoreCase)
            || sensor.Name.Contains("GPU", StringComparison.OrdinalIgnoreCase);
    }

    private static bool IsMemory(ISensor sensor)
    {
        return sensor.Name.Contains("Memory", StringComparison.OrdinalIgnoreCase)
            || sensor.Name.Contains("VRAM", StringComparison.OrdinalIgnoreCase);
    }

    private static bool Contains(string text, string value)
    {
        return text.Contains(value, StringComparison.OrdinalIgnoreCase);
    }

    public void Dispose()
    {
        computer.Close();
    }
}

internal sealed record SensorSnapshotDto
{
    [JsonPropertyName("source")]
    public string Source { get; init; } = string.Empty;

    [JsonPropertyName("timestampUtc")]
    public DateTimeOffset TimestampUtc { get; init; }

    [JsonPropertyName("processId")]
    public int ProcessId { get; init; }

    [JsonPropertyName("readings")]
    public IReadOnlyList<SensorReadingDto> Readings { get; init; } = Array.Empty<SensorReadingDto>();
}

internal sealed record RawSensorSnapshotDto
{
    [JsonPropertyName("source")]
    public string Source { get; init; } = string.Empty;

    [JsonPropertyName("timestampUtc")]
    public DateTimeOffset TimestampUtc { get; init; }

    [JsonPropertyName("processId")]
    public int ProcessId { get; init; }

    [JsonPropertyName("hardware")]
    public IReadOnlyList<RawHardwareDto> Hardware { get; init; } = Array.Empty<RawHardwareDto>();
}

internal sealed record RawHardwareDto
{
    [JsonPropertyName("name")]
    public string Name { get; init; } = string.Empty;

    [JsonPropertyName("hardwareType")]
    public string HardwareType { get; init; } = string.Empty;

    [JsonPropertyName("identifier")]
    public string Identifier { get; init; } = string.Empty;

    [JsonPropertyName("sensors")]
    public IReadOnlyList<RawSensorReadingDto> Sensors { get; init; } = Array.Empty<RawSensorReadingDto>();
}

internal sealed record RawSensorReadingDto
{
    [JsonPropertyName("name")]
    public string Name { get; init; } = string.Empty;

    [JsonPropertyName("sensorType")]
    public string SensorType { get; init; } = string.Empty;

    [JsonPropertyName("identifier")]
    public string Identifier { get; init; } = string.Empty;

    [JsonPropertyName("value")]
    public float? Value { get; init; }

    [JsonPropertyName("min")]
    public float? Min { get; init; }

    [JsonPropertyName("max")]
    public float? Max { get; init; }
}

internal sealed record SensorReadingDto
{
    [JsonPropertyName("category")]
    public string Category { get; init; } = string.Empty;

    [JsonPropertyName("label")]
    public string Label { get; init; } = string.Empty;

    [JsonPropertyName("value")]
    public double Value { get; init; }

    [JsonPropertyName("unit")]
    public string Unit { get; init; } = string.Empty;
}

internal sealed record BridgeOptions
{
    public string OutputPath { get; init; } = GetDefaultOutputPath();
    public TimeSpan Interval { get; init; } = TimeSpan.FromMilliseconds(1000);
    public bool Once { get; init; }
    public bool DumpRaw { get; init; }
    public bool DumpReport { get; init; }

    public static BridgeOptions Parse(string[] args)
    {
        var options = new BridgeOptions();
        for (var i = 0; i < args.Length; i++)
        {
            switch (args[i])
            {
                case "--output" when i + 1 < args.Length:
                    options = options with { OutputPath = args[++i] };
                    break;
                case "--interval-ms" when i + 1 < args.Length && int.TryParse(args[++i], out var intervalMs):
                    options = options with { Interval = TimeSpan.FromMilliseconds(Math.Max(250, intervalMs)) };
                    break;
                case "--once":
                    options = options with { Once = true };
                    break;
                case "--dump-raw":
                    options = options with { DumpRaw = true };
                    break;
                case "--dump-report":
                    options = options with { DumpReport = true };
                    break;
            }
        }

        return options;
    }

    private static string GetDefaultOutputPath()
    {
        var localAppData = Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData);
        return Path.Combine(localAppData, "VRPerfProfiler", "lhm-sensors.json");
    }
}
