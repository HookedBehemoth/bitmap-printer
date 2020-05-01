# bitmap-printer

"Bitmap printer goes brrrrr"

## What does this do?
This system module captures the capture button event and instead of saving a compressed (jpeg) image on the sd card it stores an uncompressed RGB bitmap on the sd card.

## Why does it need patches?
The IPC calls I use are caps:sc 1201-1203 that check a flag for debug mode. The patch makes the function call always return true.

(Should) work on HOS version 3.0.0-10.0.2.

Included patches:
| vi Version | Patch name (Build ID + .ips) | tested |
| --- | --- | --- |
| 3.0.0.410: | C41FC90700B2E371A88EA96D8E688E9AC954E40F.ips | yes |
| 3.0.1.50: | CDD21E75CDEC621583FAE6A09B7E64A4789BCD37.ips | yes* |
| 4.0.0.200: | FC5A4D49AA96C39EFEECECAAB3A4338C89A0151F.ips | yes |
| 5.0.0.430: | 7B4123290DE2A6F52DE4AB72BEA1A83D11214C71.ips | yes |
| 5.1.0.80: | 723DF02F6955D903DF7134105A16D48F06012DB1.ips | yes |
| 6.0.0.360: | 967F4C3DFC7B165E4F7981373EC1798ACA234A45.ips | yes* |
| 6.2.0.40: | 967F4C3DFC7B165E4F7981373EC1798ACA234A45.ips | yes |
| 7.0.0.200: | 98446A07BC664573F1578F3745C928D05AB73349.ips | yes |
| 8.0.0.530: | 0767302E1881700608344A3859BC57013150A375.ips | yes |
| 8.1.0.120: | 7C5894688EDA24907BC9CE7013630F365B366E4A.ips | yes |
| 9.0.0.440: | 7421EC6021AC73DD60A635BC2B3AD6FCAE2A6481.ips | yes |
| 10.0.0.300: | 96529C3226BEE906EE651754C33FE3E24ECAE832.ips | yes |

*Patch works but sysmodule doesn't. Currently unknown why.

Please post on the [issue thread](https://github.com/HookedBehemoth/bitmap-printer/issues/1) if your experience differs from the below table or if you tested an undocumented version.
