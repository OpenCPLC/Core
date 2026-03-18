# 💡 OpenCPLC

A hardware abstraction layer between your application and microcontroller peripherals. Like [Arduino](https://www.arduino.cc), but built for automation. No custom IDE, no C++. Multithreading is handled by [🔀VRTS](https://github.com/Xaeian/VRTS), cutting out typical [RTOS](https://www.freertos.org/) headaches. The system has a built-in CMD console like 🐧Linux, and [**⚒️Forge**](https://github.com/OpenCPLC/Forge) simplifies the workflow to [🐍Python](https://www.python.org/) level: install via [`pip`](https://pypi.org), clone projects with a single command.

```bash
pip install opencplc      # install Forge
opencplc -n myapp -b Uno  # new project for Uno
make run                  # build & flash
```

Technically closest to [**🪁Zephyr**](https://www.zephyrproject.org/), but simpler and closer to native solutions: plain **C** and **Makefile**, abstraction layers, mapping and configuration all in C. No external files or formats, no magic macros, no build systems that silently break IntelliSense. With a working 🐞debugger! More focused on automation than IoT. C/C++ has never gotten its own `pip` or `npm` despite decades of attempts, so we're not going that route. The framework grows steadily with semantic versioning across the whole ecosystem. One workspace, many projects, each on its own framework version, no conflicts, full reproducibility.

## C👩🏻‍❤️‍👨🏻PLC 🤔Why?

Software is getting more complex by the day, often by developers' own choice. Sometimes the complexity of an application is completely out of proportion to the problem it solves or the value it delivers. We want our solution to be as simple as possible, the interface intuitive, and the technology overhead minimal. We use well-known tools: [**Visual Studio Code**](https://code.visualstudio.com/), [**Git**](https://git-scm.com/), and [**C**](https://www.learn-c.org/), which despite its age is still [number one](https://www.geeksforgeeks.org/blogs/embedded-systems-programming-languages/) among embedded developers. There's no reason it can't show up more in industrial automation and help the industry keep pace with 🌐IT.

Demand for automation engineers has always been high and won't change. Back when programmers were scarce and automation was largely done by electricians, ladder logic **🪜LAD** was a brilliant solution, it was based on circuit logic they already understood. Today the tables have turned: C code is often more readable for technical graduates than a tree of contacts and coils. And let's not forget that [**C**](https://en.wikipedia.org/wiki/C_(programming_language)) was designed as a general-purpose language, which makes it far more versatile than the sandboxed environments shipped by PLC vendors.

_A practical comparison of LAD, ST and ANSI C can be found in the [**🟢start🔴stop**](https://github.com/OpenCPLC/Framework/wiki/Start-Stop-Lang-Comparison) example._

## 🖥️ Our Controllers

All controllers are based on the [**STM32G0**](https://www.st.com/en/microcontrollers-microprocessors/stm32g0-series.html) family and designed to make full use of the microcontroller's capabilities. They share standardized dimensions for **DIN** rail mounting and feature detachable **5.0mm** terminals for easy installation and servicing. The entire lineup is designed as a coherent platform where different models complement each other, making it easy to combine them into larger systems.

<table>
  <tr>
    <td width="50%">
      <img src="http://sqrt.pl/img/opencplc/thumbnail-uno.png">
    </td>
    <td width="50%">
      <img src="http://sqrt.pl/img/opencplc/thumbnail-eco.png">
    </td>
  </tr>
  <tr>
    <td align="center">
      The first controller in the OpenCPLC family. Versatile thanks to its wide range of peripherals. Primarily educational and demo-oriented, but perfectly capable in small production projects.
    </td>
    <td align="center">
      Small and affordable controller for autonomous operation, especially in construction machinery. Packed with potentiometers for config without a PC, and a <code>10V</code> reference voltage for joysticks and direct analog input measurement.
    </td>
  </tr>
<table>
  <tr>
    <td width="50%">
      <img src="http://sqrt.pl/img/opencplc/thumbnail-dio.png">
    </td>
    <td width="50%">
      <img src="http://sqrt.pl/img/opencplc/thumbnail-aio.png">
    </td>
  </tr>
  <tr>
    <td align="center">
      Controller for medium and large projects, used as an expansion module <i>(single communication bus)</i>. Loaded with digital I/O, a second group of transistor outputs with independent power supply, and a few analog inputs for smaller setups.
    </td>
    <td align="center">
      Controller for medium and large projects, as a main unit or expansion module when extra analog channels are needed. Plenty of analog I/O, stable power supply, and a negative voltage rail for precise measurements and signal generation.
    </td>
  </tr>
</table>

The framework exposes an abstraction layer typical for industrial automation. Instead of embedded-style GPIO _(general purpose input output)_ or ADC _(analog digital converter)_, you work with **TO**, **RO**, **DI**, **AI**, **AO**. Hardware is mapped to this layer, so adding a new controller only requires a new peripheral map to work within the ecosystem.

|   Module  | Description                                                                                           |  Uno  |  Eco  |  Dio   |  Aio   |
| :-------: | :---------------------------------------------------------------------------------------------------- | :---: | :---: | :----: | :----: |
| **`RO`**  | Relay outputs: **5A** 230VAC, 7A 30VDC. Switch cycle counter.                                        |   4   |   2   |   -    |   -    |
| **`TO`**  | Transistor outputs: **4A**. Driven by supply voltage. All support PWM mode.                           |   4   |   5   | **12** |   4    |
| **`XO`**  | Triac output: 12-230VAC. Zero-crossing detection via digital input.                                   |   2   |   -   |   -    |   -    |
| **`DI`**  | Digital inputs: **12VDC** logic high. Supports **230VAC**. Most can work as counters.                 |   4   |   4   | **12** |   4    |
| **`AI`**  | Analog inputs: **0-10V**, **4-20mA**, 0-20mA or 0-10V with voltage follower.                          |   2   |   4   |   4    | **10** |
| **`AO`**  | Analog output: **0-10V**, **0-20mA** rail-to-rail.                                                    |   -   |   -   |   -    | **4**  |
| **`RTD`** | Resistive sensor input, optimized for **PT100** and **PT1000**.                                       |   1   |   -   |   -    |   -    |
| **`RS`**  | **RS485** communication interface with **Modbus RTU**, **BACnet** or bare metal support.              |   2   |   1   |   1    |   2    |
| **`I2C`** | Communication bus with **5V** buffer and **1kΩ** pull-up.                                             |   1   |   -   |   -    |   1    |
| **`POT`** | Potentiometer. Works as internal `AI`. Allows configuration without a PC.                             |   1   | **6** |   3    |   -    |
| **`BTN`** | Button or switch. Works as internal `DI`.                                                             |   1   | **5** |   -    |   -    |
| **`LED`** | **RGB** status LED.                                                                                   |   1   |   1   |   1    |   1    |
|  `FLASH`  | Non-volatile memory **`kB`**: program, config, EEPROM emulation.                                      | `512` | `128` | `512`  | `512`  |
|   `RAM`   | Working memory **`kB`**: buffers and computation.                                                     | `144` | `36`  | `144`  | `144`  |
|   `RTC`   | Real-time clock: date and time.                                                                       |   🕑   |   -   |   🕑    |   🕑    |

## 🆚 Key Advantages

**OpenCPLC** controllers stand out in environments where typical PLCs fall short. They support standard **24VDC** automation but also **12VDC**, common in mobile machinery _(construction, agriculture)_. They measure supply voltage `VCC`, important when powering directly from a battery. They accept **230VAC** signals directly on inputs, eliminating the need for extra modules. **4A** outputs handle loads directly, and firmware **`FW`** running without an operating system **`OS`** means fast startup and rock-solid stability. Every controller ships configured as an expansion module but can be easily reprogrammed as a standalone PLC.

| PLC                |      Power supply |     DI1️⃣ | DI 230V | `TO` type | `TO` current | Get `VCC` | `FW`/`OS` |
| :----------------- | ----------------: | -------: | :-----: | --------- | :----------: | :-------: | :-------: |
| Siemens S7-1200    |  20.4-28.8V ❌    | ≥15V ❌  |    ❌    | Source    |    0.5A      |     ❌     |   `FW`    |
| Siemens S7-1500    |  19.2-28.8V ❌    | ≥15V ❌  |    ❌    | Both      |    0.5A      |     ✅     |   `FW`    |
| Mitsubishi iQ-F    |      20-28V ❌    | ≥15V ❌  |    ❌    | Both      |    0.5A      |     ✅     |   `FW`    |
| Beckhoff CX7000    |  20.4-28.8V ❌    | ≥11V ✅  |    ❌    | Source    |    0.5A      |     ❌     |   `OS`    |
| WAGO PFC200        |    18-31.2V ❌    | ≥15V ❌  |    ❌    | Both      |    0.5A      |     ❌     |   `OS`    |
| Allen-Bradley      |    10-28.8V ✅    | ≥11V ✅  |    ❌    | Source    |    0.5A      |     ❌     |   `FW`    |
| Schneider Modicon  |  20.4-28.8V ❌    | ≥15V ❌  |    ❌    | Source    |    0.5A      |     ❌     |   `FW`    |
| Phoenix Contact    |    19.2-30V ❌    | ≥11V ✅  |    ❌    | Both      |    0.5A      |     ❌     |   `OS`    |
| B&R X20            |  20.4-28.8V ❌    | ≥15V ❌  |    ❌    | Sink 💀   |    0.5A      |     ✅     |   `FW`    |
| Delta DVP-SS2      |  20.4-28.8V ❌    | ≥15V ❌  |    ❌    | Both      |    0.5A      |     ❌     |   `FW`    |
| Eaton easyE4       |  12.2-28.8V ✅    |  ≥9V ✅  |    ✅    | Both      |    0.5A      |     ❌     |   `FW`    |
| ABB AC500          |      20-30V ❌    | ≥15V ❌  |    ❌    | Both      |    0.5A      |     ✅     |   `FW`    |
| Bosch Rexroth      |      18-30V ❌    | ≥15V ❌  |    ❌    | Both      |    0.5A      |     ✅     |   `FW`    |
| Unitronics         |  10.2-28.8V ✅    | ≥15V ❌  |    ❌    | Both      |    0.5A      |     ❌     |   `FW`    |
| Turck TX500        |      10-32V ✅    | ≥12V ✅  |    ❌    | Source    |    0.5A      |     ❌     |   `OS`    |
| **OpenCPLC**       |      11-32V ✅    |  ≥9V ✅  |    ✅    | Source    |   **4A**     |     ✅     |   `FW`    |

_Data in the table is indicative. Most PLCs support expansion modules with higher current capacity or 230V signal handling. Values refer to standard digital inputs and transistor outputs._

## 🤝 Collaboration

More and more companies and engineers in the automation market are realizing that custom hardware can give them a competitive edge, solutions that scale with the business and fit the specific needs of each project. The challenge is often lack of embedded experience, the time it takes to build from scratch, and the risk that despite all the effort, the project simply doesn't pan out. OpenCPLC simplifies this with an open framework and ready-made hardware base. The whole thing can be done in a clean two-step model:

- 1️⃣ **Deploy on our reference controllers** with open firmware. Start testing ideas and developing the application right away.
- 2️⃣ **Design dedicated hardware**. This can start in parallel to reach the target solution faster, or later once the prototype is working and you want to reduce risk, or only when scaling up production.

The result: fast, tailored solutions, easy to scale through in-house manufacturing, backed by a stable framework that keeps them reliable.
