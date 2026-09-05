# -*- mode: python ; coding: utf-8 -*-


a = Analysis(
    ['2.py'],
    pathex=[],
    binaries=[
        ('OpenHardwareMonitorLib.dll', '.'),
        ('LibreHardwareMonitorLib.dll', '.'),
        ('HidSharp.dll', '.'),
        ('DiskInfoToolkit.dll', '.'),
        ('RAMSPDToolkit-NDD.dll', '.'),
        ('System.Memory.dll', '.'),
        ('System.Buffers.dll', '.'),
        ('System.Numerics.Vectors.dll', '.'),
        ('System.Runtime.CompilerServices.Unsafe.dll', '.'),
        ('System.Threading.AccessControl.dll', '.'),
        ('System.Threading.Tasks.Extensions.dll', '.'),
        ('Microsoft.Bcl.AsyncInterfaces.dll', '.'),
        ('Microsoft.Bcl.HashCode.dll', '.'),
    ],
    datas=[],
    hiddenimports=[],
    hookspath=[],
    hooksconfig={},
    runtime_hooks=[],
    excludes=[],
    noarchive=False,
    optimize=0,
)
pyz = PYZ(a.pure)

exe = EXE(
    pyz,
    a.scripts,
    a.binaries,
    a.datas,
    [],
    name='CPU_fan',
    debug=False,
    bootloader_ignore_signals=False,
    strip=False,
    upx=True,
    upx_exclude=[],
    runtime_tmpdir=None,
    console=False,
    disable_windowed_traceback=False,
    argv_emulation=False,
    target_arch=None,
    codesign_identity=None,
    entitlements_file=None,
    icon=['Only.ico'],
)
