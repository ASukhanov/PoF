# PoF
# Configuration
To work with PoF user should be in dialout group:<br>
```sudo usermod –aG dialout user```

Low level test using python miniterm:<br>
```python3 -m serial.tools.miniterm /dev/ttyUSB0 115200
<M7,7,-2561,8,2,7193449>
<R-257>
...
```
