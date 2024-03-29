# bitmap-printer
"Bitmap printer goes brrrrr"

## What does this do?
This system module captures the capture button event and instead of saving a compressed (jpeg) image, it stores an uncompressed RGB bitmap on the microSD card.

## Why does it need patches?
The IPC calls I use are caps:sc 1201-1203 that check a flag for debug mode. The patch makes the function call always return true.

Works on HOS versions 3.0.0-18.0.0 (2024.03.26).

Included patches:
| vi Version | Patch name (Build ID + .ips) | tested |
| --- | --- | --- |
| 3.0.0.410 | C41FC90700B2E371A88EA96D8E688E9AC954E40F.ips | yes |
| 3.0.1.50 | CDD21E75CDEC621583FAE6A09B7E64A4789BCD37.ips | yes |
| 4.0.0.200 | FC5A4D49AA96C39EFEECECAAB3A4338C89A0151F.ips | yes |
| 5.0.0.430 | 7B4123290DE2A6F52DE4AB72BEA1A83D11214C71.ips | yes |
| 5.1.0.80 | 723DF02F6955D903DF7134105A16D48F06012DB1.ips | yes |
| 6.0.0.360 | 967F4C3DFC7B165E4F7981373EC1798ACA234A45.ips | yes |
| 6.2.0.40 | 967F4C3DFC7B165E4F7981373EC1798ACA234A45.ips | yes |
| 7.0.0.200 | 98446A07BC664573F1578F3745C928D05AB73349.ips | yes |
| 8.0.0.530 | 0767302E1881700608344A3859BC57013150A375.ips | yes |
| 8.1.0.120 | 7C5894688EDA24907BC9CE7013630F365B366E4A.ips | yes |
| 9.0.0.440 | 7421EC6021AC73DD60A635BC2B3AD6FCAE2A6481.ips | yes |
| 10.0.0.300 | 96529C3226BEE906EE651754C33FE3E24ECAE832.ips | yes |
| 11.0.0.400 | D689E9FAE7CAA4EC30B0CD9B419779F73ED3F88B.ips | yes |
| 11.0.1.20 | 65A23B52FCF971400CAA4198656D73867D7F1F1D.ips | yes |
| 12.0.0.2500 | B295D3A8F8ACF88CB0C5CE7C0488CC5511B9C389.ips | yes |
| 12.1.0.120 | B295D3A8F8ACF88CB0C5CE7C0488CC5511B9C389.ips | yes |
| 13.0.0.220 | 82EE58BEAB54C1A9D4B3D9ED414E84E31502FAC6.ips | yes |
| 14.0.0.1240 | AFEAACF3E88AB539574689D1458060657E81E088.ips | yes |
| 15.0.0.1120 | 7E9BB552AAEFF82363D1E8C97B5C6B95E3989E1A.ips | yes |
| 16.0.0.1040 | BA15B407573B8CECF0FAE2B367D3103A2A1E821C.ips | yes |
| 17.0.0.960 | 34D15383767E313EE76F1EE991CD00AD2BF8C62A.ips | yes |
| 18.0.0.1060 | 9D8D6EFEE01E97E95E00D573530C09CB5AB5B8A0.ips | yes |

Please post on the [issue thread](https://github.com/HookedBehemoth/bitmap-printer/issues/1) if your experience differs from the table above or if you tested an undocumented version.
