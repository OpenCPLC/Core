# 💡 OpenC𝐏𝐋𝐂

Projekt to warstwa abstrakcji między aplikacją a peryferiami mikrokontrolera. Podobnie jak **Arduino**, ale ukierunkowana na automatykę. Bez własnego IDE i C++. Wielowątkowość zapewnia [**🔀VRTS**](https://github.com/Xaeian/VRTS), eliminując typowe problemy **RTOS**. System ma wbudowaną konsolę CMD jak **🐧Linux**, a [🔮Wizard](https://github.com/OpenCPLC/Wizard) upraszcza programowanie mikrokontrolerów do poziomu znanego z [**🐍Python**'a](https://www.python.org/). Technologicznie najbliżej mu do [**🪁Zephyr**'a](https://www.zephyrproject.org/), ale jest prostszy, bliższy natywnym rozwiązaniom, z działającym debuggerem i skupiony na automatyce zamiast IoT.

W świecie technologii, programowanie staje się coraz bardziej złożone _(często na życzenie samych programistów)_. Niekiedy poziom skomplikowania aplikacji jest nieproporcjonalny do problemu, który rozwiązuje lub wartości, jaką dostarcza. Chcemy, aby nasze rozwiązanie były możliwie proste, interfejs intuicyjny, a nakład technologiczny minimalny. Wykorzystujemy dobrze znane narzędzia, takie jak [**Visual Studio Code**](https://code.visualstudio.com/), system kontroli wersji [**Git**](https://git-scm.com/) oraz język [**C**](https://www.learn-c.org/pl/), który pomimo swojego wieku nadal jest [numerem jeden](https://www.geeksforgeeks.org/blogs/embedded-systems-programming-languages/) wśród programistów Embedded. Nic nie stoi więc na przeszkodzie, aby pojawiło się go trochę więcej w automatyce, co pozwoli iść branży z duchem 🌐IT!

Zapotrzebowanie na automatyków było, jest i będzie bardzo duże. W przeszłości, kiedy programistów było niewielu, a za automatykę brali się głównie elektrycy, zaprojektowanie języka drabinkowego ladder logic **🪜LAD** było strzałem w dziesiątkę! Opierał się bowiem na logice znanej z układów elektrycznych. Dziś sytuacja się odwróciła: kod w języku **C** jest często bardziej czytelny dla absolwentów kierunków technicznych niż drzewo logiczne ze styków i cewek. Nie zapominajmy, że język [**C**](<https://pl.wikipedia.org/wiki/C_(j%C4%99zyk_programowania)>) powstał jako język ogólnego przeznaczenia, dlatego charakteryzuje się dużą uniwersalnością, szczególnie w porównaniu do sandbox'ów dostarczanych przez producentów sterowników PLC.

_Praktyczne porównanie języków LAD, ST i ANSI C można zobaczyć na przykładzie systemu [**🟢start🔴stop**](https://github.com/OpenCPLC/Framework/wiki/Start-Stop-Lang-Comparison)._   

## 🖥️ Our Controllers

Wszystkie sterowniki bazują na rodzinie [**STM32G0**](https://www.st.com/en/microcontrollers-microprocessors/stm32g0-series.html) i zostały zaprojektowane tak, aby w pełni wykorzystać potencjał mikrokontrolera. Mają ustandaryzowane wymiary dopasowane do montażu na szynie **DIN**. Wyposażono je w rozłączne terminale **5.0mm**, co ułatwia instalacje oraz serwis. Cała linia została pomyślana jako spójna platforma, gdzie różne modele uzupełniają się funkcjonalnie, dzięki czemu można je łatwo łączyć w większe systemy.

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
      Pierwszy sterownik z rodziny OpenCPLC. Cechuje się wszechstronnością ze względu na różnorodność peryferii. Choć pełni funkcję demonstracyjno-edukacyjną, z powodzeniem sprawdzi się również w małych projektach.
    </td>
    <td align="center">
      Mały i tani sterownik do pracy autonomicznej, szczególnie w maszynach budowlanych. Wyposażony w wiele potencjometrów dla konfiguracji bez komputera oraz napięcie referencyjne <code>10V</code> do obsługi joysticków i bezpośredniego pomiaru na wejściach analogowych.
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
      Sterownik do średnich i dużych projektów, jako moduł rozszerzeń <i>(jedna magistrala komunikacyjna)</i>. Wyposażony w liczne wejścia i wyjścia cyfrowe, z drugą grupą wyjść tranzystorowych zasilanych niezależnie od głównego. Ma też kilka wejść analogowych, przydatnych w mniejszych projektach.
    </td>
    <td align="center">
      Sterownik do średnich i dużych projektów. Jako jednostka główna lub moduł rozszerzeń, gdy potrzebne są dodatkowe kanały analogowe. Posiada wiele wejść i wyjść analogowych, stabilne zasilanie oraz napięcie ujemne dla dokładniejszych pomiarów i generacji sygnałów.
    </td>
  </tr>
</table>

Framework udostępnia warstwę abstrakcji typową dla automatyki. Zamiast znanych z embedded GPIO _(general purpose input output)_ czy ADC _(analog digital converter)_ pracujemy z **TO**, **RO**, **DI** oraz **AI**, **AO**. Sprzęt jest mapowany do tej warstwy, więc każdy nowy sterownik wymaga tylko nowej mapy peryferiów, aby działać w ekosystemie.

|   Układ   | Opis                                                                                                  |  Uno  |  Eco  |  Dio   |  Aio   |
| :-------: | :---------------------------------------------------------------------------------------------------- | :---: | :---: | :----: | :----: |
| **`RO`**  | Wyjścia przekaźnikowe: **5A** 230VAC, 7A 30VDC. Licznik przełączeń.                                   |   4   |   2   |   -    |   -    |
| **`TO`**  | Wyjścia tranzystorowe: **4A**. Sterowane napięciem zasilania. Wszystkie mogą pracować w trybie PWM.   |   4   |   5   | **12** |   4    |
| **`XO`**  | Wyjście triakowe: 12-230VAC. Detekcja przejścia przez zero z wykorzystaniem wejścia cyfrowego.        |   2   |   -   |   -    |   -    |
| **`DI`**  | Wejścia cyfrowe: od **12VDC**  stan wysoki. Obsługa **230VAC**. Większość może działać jako liczniki. |   4   |   4   | **12** |   4    |
| **`AI`**  | Wejścia analogowe: **0-10V**, **4-20mA**, 0-20mA lub 0-10V z wtórnikiem napięciowym.                  |   2   |   4   |   4    | **10** |
| **`AO`**  | Wyjście analogowe: **0-10V**, **0-20mA** rail-to-rail.                                                |   -   |   -   |   -    | **4**  |
| **`RTD`** | Wejście czujników rezystancyjnych, dostosowane do **PT100** i **PT1000**.                             |   1   |   -   |   -    |   -    |
| **`RS`**  | Interfejs komunikacyjny **RS485** z obsługą **Modbus RTU**, **BACnet** lub bare metal.                |   2   |   1   |   1    |   2    |
| **`I2C`** | Magistra komunikacyjna z buforem **5V** i pull-up **1kΩ**.                                            |   1   |   -   |   -    |   1    |
| **`POT`** | Potencjometr.  Działa jak wewnętrzne `AI`. Umożliwia konfigurację bez użycia komputera.               |   1   | **6** |   3    |   -    |
| **`BTN`** | Przycisk lub przełącznik. Działa jak wewnętrzne `DI`.                                                 |   1   | **5** |   -    |   -    |
| **`LED`** | Dioda informacyjna **RGB**.                                                                           |   1   |   1   |   1    |   1    |
|  `FLASH`  | Pamięć nieulotna **`kB`**: program, konfiguracja, emulacja EEPROM.                                    | `512` | `128` | `512`  | `512`  |
|   `RAM`   | Pamięć operacyjna **`kB`**: bufory i obliczenia.                                                      | `144` | `36`  | `144`  | `144`  |
|   `RTC`   | Zegar czasu rzeczywistego: data i godzina.                                                            |   🕑   |   -   |   🕑    |   🕑    |

## 🆚 Key Advantages

Sterowniki **OpenCPLC** wyróżniają się możliwością pracy w środowiskach, gdzie typowe PLC zawodzą. Obsługują standardową automatykę **24VDC**, ale także **12VDC**, typowe dla maszyn mobilnych w przemyśle _(np. budowlanym czy rolniczym)_. Zapewniają pomiar napięcia zasilania `VCC`, co ma znaczenie przy zasilaniu sterownika bezpośrednio z akumulatora. Przyjmują bezpośrednie sygnały **230VAC** na wejściach, co eliminuje konieczność stosowania dodatkowych modułów. Wyjścia **4A** pozwalają sterować obciążeniami bezpośrednio, a firmware **`FW`** wgrywany bez systemu operacyjnego **`OS`** zapewnia szybki start i wysoką stabilność pracy. Każdy sterownik jest fabrycznie skonfigurowany jako moduł rozszerzeń, ale można go łatwo przeprogramować do pracy jako samodzielny PLC.

| Sterownik PLC     |    Zasilanie |    DI1️⃣ | DI 230V | Typ `TO` | Prąd `TO` | Get `VCC` | `FW`/`OS` |
| :---------------- | -----------: | -----: | :-----: | -------- | :-------: | :-------: | :-------: |
| Siemens S7-1200   | 20.4-28.8V ❌ | ≥15V ❌ |    ❌    | Source   |   0.5A    |     ❌     |   `FW`    |
| Siemens S7-1500   | 19.2-28.8V ❌ | ≥15V ❌ |    ❌    | Both     |   0.5A    |     ✅     |   `FW`    |
| Mitsubishi iQ-F   |     20-28V ❌ | ≥15V ❌ |    ❌    | Both     |   0.5A    |     ✅     |   `FW`    |
| Beckhoff CX7000   | 20.4-28.8V ❌ | ≥11V ✅ |    ❌    | Source   |   0.5A    |     ❌     |   `OS`    |
| WAGO PFC200       |   18-31.2V ❌ | ≥15V ❌ |    ❌    | Both     |   0.5A    |     ❌     |   `OS`    |
| Allen-Bradley     |   10-28.8V ✅ | ≥11V ✅ |    ❌    | Source   |   0.5A    |     ❌     |   `FW`    |
| Schneider Modicon | 20.4-28.8V ❌ | ≥15V ❌ |    ❌    | Source   |   0.5A    |     ❌     |   `FW`    |
| Phoenix Contact   |   19.2-30V ❌ | ≥11V ✅ |    ❌    | Both     |   0.5A    |     ❌     |   `OS`    |
| B&R X20           | 20.4-28.8V ❌ | ≥15V ❌ |    ❌    | Sink 💀   |   0.5A    |     ✅     |   `FW`    |
| Delta DVP-SS2     | 20.4-28.8V ❌ | ≥15V ❌ |    ❌    | Both     |   0.5A    |     ❌     |   `FW`    |
| Eaton easyE4      | 12.2-28.8V ✅ |  ≥9V ✅ |    ✅    | Both     |   0.5A    |     ❌     |   `FW`    |
| ABB AC500         |     20-30V ❌ | ≥15V ❌ |    ❌    | Both     |   0.5A    |     ✅     |   `FW`    |
| Bosch Rexroth     |     18-30V ❌ | ≥15V ❌ |    ❌    | Both     |   0.5A    |     ✅     |   `FW`    |
| Unitronics        | 10.2-28.8V ✅ | ≥15V ❌ |    ❌    | Both     |   0.5A    |     ❌     |   `FW`    |
| Turck TX500       |     10-32V ✅ | ≥12V ✅ |    ❌    | Source   |   0.5A    |     ❌     |   `OS`    |
| **OpenCPLC**      |     11-32V ✅ |  ≥9V ✅ |    ✅    | Source   |  **4A**   |     ✅     |   `FW`    |

_Dane w tabeli są poglądowe. Większość sterowników umożliwia rozbudowę o dodatkowe moduły, np. z wyższą wydajnością prądową lub do obsługi sygnałów 230V. Wartości odnoszą się do standardowych wejść cyfrowych i wyjść tranzystorowych._

## 🤝 Collaboration

Na rynku automatyki coraz więcej firm i inżynierów dostrzega, że własne konstrukcje mogą dać im przewagę rynkową. Takie rozwiązania można skalować wraz z rozwojem biznesu oraz dopasować do specyfiki projektu. Problemem może być brak doświadczenia w embedded, długi czas tworzenia rozwiązania od podstaw oraz ryzyko, że pomimo pochłoniętych zasobów projekt po prostu się nie uda. OpenCPLC upraszcza ten proces, oferując otwarty framework i gotową bazę sprzętową. Całość można zrealizować w przejrzystym, dwuetapowym modelu:

- 1️⃣ **Wdrożenie projektu na naszych sterownikach** referencyjnych z otwartym firmware. Pozwala od razu testować pomysły i rozwijać aplikację.
- 2️⃣ **Projekt dedykowanego hardware**. Można go rozpocząć równolegle, aby szybciej dojść do rozwiązania docelowego, później, gdy prototyp zacznie działać i chcemy ograniczyć ryzyko, lub dopiero w momencie zwiększenia nakładów.

W ten sposób powstają rozwiązania szybkie i dopasowane, łatwe do skalowania dzięki własnej produkcji, a stabilny framework zapewnia im wysoką niezawodność.
