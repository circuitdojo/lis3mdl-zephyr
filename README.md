# ST LIS3MDL for Zephyr

## Use

Incude this project in your `west.yml` to use:

```
manifest:
  projects:
    # LIS3MDL driver
    - name: lis3mdl
      path: lis3mdl
      revision: main
      url: https://github.com/circuitdojo/lis3mdl-zephyr.git
```

Enable in your `prj.conf`

```
CONFIG_SENSOR=y
CONFIG_LIS3MDL_CD=y
```

Get the binding in your code and go to town:

```
const struct device *dev = DEVICE_DT_GET_ANY(DT_NODELABEL(st_lis3mdl_magn));
```