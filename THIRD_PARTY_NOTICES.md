# Third-Party Notices

VR Performance Profiler includes or links against third-party software. SteamVR
itself is not bundled.

## OpenVR SDK

- Source: https://github.com/ValveSoftware/openvr
- Version: v2.5.1
- License: BSD 3-Clause
- Used for SteamVR/OpenVR overlay APIs and `openvr_api.dll`.

Copyright (c) 2015, Valve Corporation
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice, this
list of conditions and the following disclaimer.
2. Redistributions in binary form must reproduce the above copyright notice,
this list of conditions and the following disclaimer in the documentation
and/or other materials provided with the distribution.
3. Neither the name of the copyright holder nor the names of its contributors
may be used to endorse or promote products derived from this software without
specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

## nlohmann/json

- Source: https://github.com/nlohmann/json
- Version: v3.11.3
- License: MIT
- Used for JSON parsing and serialization.

Copyright (c) 2013-2022 Niels Lohmann

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.

## Microsoft WebView2

- Package: Microsoft.Web.WebView2 1.0.4022.49
- Publisher: Microsoft
- License file: `LICENSE.txt` in the NuGet package
- Used for the settings window loader DLL.

## Microsoft .NET Runtime

- Runtime: Microsoft .NET 8 win-x64 runtime components
- Publisher: Microsoft
- Notices: Microsoft .NET runtime packages include `LICENSE` and
  `THIRD-PARTY-NOTICES.TXT` in the NuGet runtime packs.
- Used by the self-contained LibreHardwareMonitor bridge helper.

## LibreHardwareMonitorLib

- Package: LibreHardwareMonitorLib 0.9.6
- Source: https://github.com/LibreHardwareMonitor/LibreHardwareMonitor
- License: MPL-2.0
- Used by the optional hardware temperature bridge helper.

## LibreHardwareMonitorLib transitive packages

The self-contained bridge may include transitive NuGet dependencies from
LibreHardwareMonitorLib, including:

- DiskInfoToolkit 1.1.2
- HidSharp 2.6.4
- Mono.Posix.NETStandard 1.0.0
- RAMSPDToolkit-NDD 1.4.2
- System.CodeDom
- System.IO.Ports
- System.Management
- System.Threading.AccessControl

See the corresponding NuGet package metadata and included license/notice files
for each package's terms.
