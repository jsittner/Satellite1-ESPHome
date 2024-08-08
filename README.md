## Variants
#### Full-feature Voice Assistant
`satellite_va_core_r2.yaml`

`satellite_va_core_r1.yaml`

(right now it is not necessary to have separate variants for hat rev.1/rev.2 )

#### Testing / Debugging 
These configs don't get build automatically by GH-Actions.


`test_core_boards.yaml`

`test_stream_mics_udp.yaml`

## Flashing the ESP32

Create/activate environment by running from project root:
```bash
source scripts/setup_build_env.sh
```

build firmware:
```bash
esphome compile config/satellite_mic_test.yaml
```

upload firmware:
```bash
esphome upload config/satellite_mic_test.yaml
```

setup wifi:

go to: https://web.esphome.io/?dashboard_wizard

click on connet and select the usb port 

click on the three dots and select 'Configure WiFi'

check the logs:
```bash
esphome logs config/satellite_mic_test.yaml
```




## Setting up Home Assistant
Setup voice assistant pipeline:

https://www.home-assistant.io/voice_control/voice_remote_local_assistant/


enable recording of voice assistant streams:

add to your configuration.yaml:
```yaml
assist_pipeline:
  debug_recording_dir: /config/debug
```
