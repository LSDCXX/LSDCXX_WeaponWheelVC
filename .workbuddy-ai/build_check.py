import subprocess, os, sys

MSBUILD = "Q:\\Microsoft Visual Studio\\2022\\Community\\MSBuild\\Current\\Bin\\MSBuild.exe"
PROJ_DIR = r"Q:\Lee\Documents\GitHub\LSDCXX_WeaponWheelVC"
SLN = os.path.join(PROJ_DIR, "LSDCXX_WeaponWheelVC.sln")

env = dict(os.environ)
env["PLUGIN_SDK_DIR"] = r"Q:\Lee\Documents\GitHub\plugin-sdk"

r = subprocess.run(
    [MSBUILD, SLN, "/t:Rebuild", "/p:Configuration=Release GTA-VC",
     "/p:Platform=Win32", "/v:normal", "/nologo"],
    capture_output=True, text=True, cwd=PROJ_DIR, env=env,
)

out = r.stdout
issues = [ln for ln in out.splitlines()
          if ("warning" in ln.lower() or "error" in ln.lower())]

print("RC:", r.returncode)
print("ISSUE_LINES:", len(issues))
for ln in issues[:60]:
    print(ln)
if r.stderr.strip():
    print("STDERR:", r.stderr[-3000:])
