# aero-cli

![CodeRabbit Pull Request Reviews](https://img.shields.io/coderabbit/prs/github/airframesio/acars-decoder-typescript)
[![Contributors](https://img.shields.io/github/contributors/airframesio/aero-cli)](https://github.com/airframesio/aero-cli/graphs/contributors)
[![Activity](https://img.shields.io/github/commit-activity/m/airframesio/aero-cli)](https://github.com/airframesio/aero-cli/pulse)
[![Discord](https://img.shields.io/discord/1067697487927853077?logo=discord)](https://discord.gg/8Ksch7zE)

## Example
Running `aero-cli` is similar to running SDRReceiver and JAERO: `aero-publish` is interchangeable with SDRReceiver and `aero-decode` is interchangeable with JAERO.

To run `aero-publish`, make sure to at the minimum include the SoapySDR driver string `driver=rtlsdr` with an optional `,serial=NNNNNNNN` appended for binding with specific SDR in the event of many. `sdr_54W_all.ini` can be found in `samples_ini/` withint the SDRReceiver repository.
```bash
aero-publish -d driver=rtlsdr --enable-biast sdr_54W_all.ini
```

To run `aero-decode`:
```bash
aero-decode -v -p tcp://127.0.0.1:6004 -t VFO52 -b 10500 -f jsondump=tcp://127.0.0.1:4444
```

## TODO
- [x] Implement C-band support (1200/10500)
- [x] Implement test harness that streams audio from audio-out into a ZeroMQ topic for samples testing (mostly for burst mode)
- [x] Cut out plane registration database code from AeroL
- [x] Implement ACARS frame forwarding functionality
- [x] Implement translation of ACARSItem to JSON
- [x] libacars integration
- [ ] SBS forwarding
- [ ] AMBE decoding?
- [ ] Long term test to ensure processor and memory usage is within expectations

## Requirements
Other configurations not mentioned may work but below is the configuration used for development and testing:
* SoapySDR 0.8.1
* ZeroMQ 4.3.5
* libcorrect (commit f5a28c74fba7a99736fe49d3a5243eca29517ae9)
* QT 6.4+ (Core, Concurrent, Multimedia)

Unfortunately, the audio test harness requires Qt 5 in order to gain access to `monitor` audio inputs:
 * Python 3
 * PyQt5
 * PyQt5 QtMultimedia
 * PyZMQ

## Compiling

```
git clone https://github.com/airframesio/aero-cli.git
cd aero-cli
mkdir build && cd build
cmake ..
```

```
make && make install
```

## Credits
* JAERO team
* SDRReceiver team
