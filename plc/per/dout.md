






## 📟 Bash




```bash
DOUT list
```


```bash
DOUT <output> {set|rst|tgl} 
DOUT RO1 set
```



```bash
DOUT <output> pulse <count> <ton_ms> <toof_ms> 
DOUT TO1 pulse 500
```




```bash
DOUT <output> duty [0..100%] 
DOUT TO1 duty 50%
```


```bash
DOUT <output> freq <frequency_Hz> 
DOUT TO1 freq 1kHz
```