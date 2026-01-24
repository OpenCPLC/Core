# task

Scheduler opóźnionych zadań dla VRTS.

## Przykład - proste opóźnienie
```c
void led_off(void *arg) {
  LED_Set(0);
}

void button_pressed(void) {
  LED_Set(1);
  TASK_Add(led_off, NULL, 1000);  // wyłącz LED po 1s
}
```

## Przykład - timeout z możliwością restartu
```c
#define KEY_TIMEOUT 1

void connection_timeout(void *arg) {
  Conn_t *conn = arg;
  conn->state = DISCONNECTED;
  LOG_Warn("Connection timeout!");
}

void connection_start(Conn_t *conn) {
  conn->state = CONNECTED;
  // timeout 5s - jeśli nie będzie aktywności to rozłącz
  TASK_AddKey(TASK_ connection_timeout, conn, 5000, KEY_TIMEOUT);
}

void on_data_received(void) {
  // dane przyszły - restartuj timeout
  TASK_Reschedule(KEY_TIMEOUT, 5000);
}

void connection_close(void) {
  // zamykamy - anuluj timeout
  TASK_Cancel(KEY_TIMEOUT);
}
```

## Przykład - debounce przycisku
```c
#define KEY_DEBOUNCE 2

void button_confirmed(void *arg) {
  LOG_Info("Button CONFIRMED");
  do_action();
}

void button_isr(void) {
  // każde naciśnięcie restartuje timer
  // dopiero jak 50ms minie bez zmian to odpali button_confirmed
  if(TASK_Exists(KEY_DEBOUNCE)) {
    TASK_Reschedule(KEY_DEBOUNCE, 50);
  }
  else {
    TASK_AddKey(button_confirmed, NULL, 50, KEY_DEBOUNCE);
  }
}
```

## Przykład - anuluj wszystkie taski danego typu
```c
void sensor_read(void *arg);

// gdzieś dodajesz wiele tasków sensor_read
TASK_Add(TASK_ sensor_read, &sensor1, 100);
TASK_Add(TASK_ sensor_read, &sensor2, 200);
TASK_Add(TASK_ sensor_read, &sensor3, 300);

// potem chcesz anulować wszystkie
uint16_t cancelled = TASK_CancelHandler(TASK_ sensor_read);
LOG_Info("Anulowano %d tasków", cancelled);
```

## TASK_ macro
```c
void my_handler(MyStruct_t *s);

// kompilator marudzi o typach - użyj TASK_
TASK_Add(TASK_ my_handler, &my_struct, 100);
```

## Config
```c
#define TASK_LIMIT 16  // max tasków w kolejce
```

## API

| Funkcja                        | Opis                         |
| ------------------------------ | ---------------------------- |
| `TASK_Add(h, arg, ms)`         | Dodaj task                   |
| `TASK_AddKey(h, arg, ms, key)` | Dodaj z kluczem (unique)     |
| `TASK_Cancel(key)`             | Anuluj po kluczu             |
| `TASK_CancelHandler(h)`        | Anuluj wszystkie z handlerem |
| `TASK_Exists(key)`             | Czy task czeka?              |
| `TASK_Reschedule(key, ms)`     | Zmień czas                   |
| `TASK_Pending()`               | Ile tasków czeka             |
| `TASK_ClearAll()`              | Anuluj wszystko              |
| `TASK_Main()`                  | Główna pętla (blokująca)     |

## Zachowanie

- `delay_ms = 0` → natychmiastowe wykonanie (bez kolejki)
- `key = 0` → bez deduplikacji
- ten sam `key` → odrzucony (już jest w kolejce)
- handler wywoływany po zdjęciu z kolejki (safe do reschedule siebie)