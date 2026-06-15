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
    bridge.WriteSnapshot();
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
        var readings = new List<SensorReadingDto>();
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
            Readings = readings
        };

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

    private static void AddReadings(IHardware hardware, List<SensorReadingDto> readings)
    {
        foreach (var sensor in hardware.Sensors)
        {
            var reading = ConvertSensor(hardware, sensor);
            if (reading is not null)
            {
                readings.Add(reading);
            }
        }

        foreach (var subHardware in hardware.SubHardware)
        {
            AddReadings(subHardware, readings);
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
