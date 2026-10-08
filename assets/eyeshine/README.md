# Eyeshine masks

These 21 grayscale PNG files are the editable source for the eyeshine masks.
White marks the eye region and black excludes the surrounding surface.

Edit a PNG in any image editor while retaining its dimensions and grayscale
8-bit mode. Install Pillow and convert the files from the repository root:

```powershell
python -m pip install Pillow
python tools/ConvertEyeshineMasks.py
```

Conversion writes uncompressed `R8_UNORM` DDS files with one mip level to
`package/Shaders/NightEye/Eyeshine`. Use `--check` to compare generated DDS
bytes with the existing files without changing them.

The source masks match the version 1.0 DDS files byte for byte after conversion.
