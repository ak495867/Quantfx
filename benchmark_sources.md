# Benchmark data provenance

The benchmark uses real historical generic ASCII tick archives from HistData.com. The source pages inspected during the run identify tick data for EURUSD, GBPUSD, USDJPY, and AUDUSD and provide month-level download pages for January 2024.

The source FAQ states that generic tick files contain `DateTime,Bid,Ask,Volume`, are time ordered, and use Eastern Standard Time without daylight-saving adjustment. The downloaded files were converted to Parquet with a fixed UTC-05:00 interpretation.

| Pair | Source page | Archive | Download SHA-256 |
|---|---|---|---|
| EURUSD | https://www.histdata.com/download-free-forex-historical-data/?/ascii/tick-data-quotes/eurusd/2024/1 | HISTDATA_COM_ASCII_EURUSD_T_202401.zip | 9a4ae15d87abff93311cc60adf7fe83de9078664c6baaf2984fcc70f160b3da9 |
| GBPUSD | https://www.histdata.com/download-free-forex-historical-data/?/ascii/tick-data-quotes/gbpusd/2024/1 | HISTDATA_COM_ASCII_GBPUSD_T_202401.zip | 9d6ad5449985d0bcc7e21b1a1c1ed136208b91329d6cf5b975f3ec87bace566d |
| USDJPY | https://www.histdata.com/download-free-forex-historical-data/?/ascii/tick-data-quotes/usdjpy/2024/1 | HISTDATA_COM_ASCII_USDJPY_T_202401.zip | 2266c00451e5c8089dfa46bb35b31bea98578ebfca9697bae4817bcc8219b3ef |
| AUDUSD | https://www.histdata.com/download-free-forex-historical-data/?/ascii/tick-data-quotes/audusd/2024/1 | HISTDATA_COM_ASCII_AUDUSD_T_202401.zip | ca6aefdea014475db9407fe88d8421e0c91d5500685d71b96973aef86ccd0812 |

The provider’s FAQ also states that the free data carries no warranty or certification and that the source data reflects broker/provider-specific quote characteristics. The benchmark therefore treats the feeds as one historical reference, not a consolidated institutional FX tape.

References:

1. [HistData FAQ](https://www.histdata.com/f-a-q/)
2. [HistData generic tick-data selection](https://www.histdata.com/download-free-forex-data/?/ascii/tick-data-quotes)
3. [HistData EURUSD January 2024 tick page](https://www.histdata.com/download-free-forex-historical-data/?/ascii/tick-data-quotes/eurusd/2024/1)

The browser-verified download flow is: pair selector `https://www.histdata.com/download-free-forex-data/?/ascii/tick-data-quotes`, pair/year selector pages under `https://www.histdata.com/download-free-forex-historical-data/?/ascii/tick-data-quotes/{pair}/{year}`, and month pages under the same path with `/{month}`. The January 2024 EURUSD page displayed the archive name `HISTDATA_COM_ASCII_EURUSD_T_202401.zip`. The actual POST download required the page token, referer, session cookies, and form fields `tk`, `date`, `datemonth`, `platform=ASCII`, `timeframe=T`, and `fxpair`.
