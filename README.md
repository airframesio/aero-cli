# aero-cli

![CodeRabbit Pull Request Reviews](https://img.shields.io/coderabbit/prs/github/airframesio/acars-decoder-typescript)
[![Contributors](https://img.shields.io/github/contributors/airframesio/aero-cli)](https://github.com/airframesio/aero-cli/graphs/contributors)
[![Activity](https://img.shields.io/github/commit-activity/m/airframesio/aero-cli)](https://github.com/airframesio/aero-cli/pulse)
[![Discord](https://img.shields.io/discord/1067697487927853077?logo=discord)](https://discord.gg/8Ksch7zE)

TBA

## TODO
- [ ] Implement test harness that streams audio from audio-out into a ZeroMQ topic for samples testing
- [x] Cut out plane registration database code from AeroL
- [ ] Implement ACARS frame forwarding functionality
- [x] Implement translation of ACARSItem to JSON
- [ ] Long term test to ensure processor and memory usage is within expectations

## Requirements
Other configurations not mentioned may work but below is the configuration used for development and testing:
* SoapySDR 0.8.1
* ZeroMQ 4.3.5
* libcorrect (commit f5a28c74fba7a99736fe49d3a5243eca29517ae9)
* QT 6.4+

## Credits
* JAERO team
