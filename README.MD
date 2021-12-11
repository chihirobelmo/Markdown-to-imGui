# prepare

you need to install stb package to run this program.
open up power shell and type this.

```powershell
git clone https://github.com/Microsoft/vcpkg.git
cd vcpkg
.\bootstrap-vcpkg.bat
.\vcpkg.exe install stb
.\vcpkg.exe install stb:x64-windows
.\vcpkg.exe integrate install
```