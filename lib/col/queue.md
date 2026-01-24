# queue

Kolejka priorytetowa. Bazuje na `ary_t`.

## Przykład - prosta kolejka zadań z priorytetami
```c
typedef struct {
  uint8_t priority;
  void (*Run)(void);
} Job_t;

// sortowanie: mniejszy priority = ważniejszy
int job_cmp(const void *a, const void *b) {
  return ((Job_t*)a)->priority - ((Job_t*)b)->priority;
}

QUEUE_New(jobs, Job_t, 8);
jobs.Compare = job_cmp;

Job_t j;
j.priority = 3; j.Run = led_blink;
QUEUE_Push(&jobs, &j);
j.priority = 1; j.Run = send_uart;  // ważniejsze
QUEUE_Push(&jobs, &j);
j.priority = 2; j.Run = read_sensor;
QUEUE_Push(&jobs, &j);

Job_t next;
while(!QUEUE_IsEmpty(&jobs)) {
  QUEUE_Pop(&jobs, &next);
  LOG_Info("Priority %d", next.priority);  // 1, 2, 3
  next.Run();
}
```

## Przykład - unique (bez duplikatów)
```c
typedef struct {
  int32_t id;
  uint32_t value;
} Event_t;

bool event_eq(const void *a, const void *b) {
  return ((Event_t*)a)->id == ((Event_t*)b)->id;
}

QUEUE_New(events, Event_t, 16);
events.unique = true;
events.Equal = event_eq;

Event_t e;
e.id = 1; e.value = 100;
QUEUE_Push(&events, &e);  // ok
e.id = 2; e.value = 200;
QUEUE_Push(&events, &e);  // ok
e.id = 1; e.value = 300;  // id=1 już jest, nie wejdzie
QUEUE_Push(&events, &e);  // zwraca false

LOG_Info("Count: %d", QUEUE_Count(&events));  // 2
```

## Przykład - kasowanie po kluczu
```c
Event_t key = { .id = 2 };
if(QUEUE_Remove(&events, &key, NULL)) {
  LOG_Info("Usunięto event id=2");
}

// albo usuń wszystkie spełniające warunek
bool is_old(const void *el, void *ctx) {
  return ((Event_t*)el)->value < 150;
}
uint16_t removed = QUEUE_RemoveAll(&events, is_old, NULL);
```

## Config
```c
#define QUEUE_USE_HEAP 0  // insertion sort O(n) - domyślne, proste
#define QUEUE_USE_HEAP 1  // heap O(log n) - dla dużych kolejek
```

## API

| Funkcja           | Opis                                |
| ----------------- | ----------------------------------- |
| `QUEUE_Push`      | Dodaj (z sortowaniem jeśli Compare) |
| `QUEUE_Pop`       | Zdejmij najważniejszy               |
| `QUEUE_Peek`      | Podglądnij najważniejszy            |
| `QUEUE_Find`      | Znajdź index (-1 jeśli brak)        |
| `QUEUE_Remove`    | Usuń po kluczu                      |
| `QUEUE_RemoveAt`  | Usuń po indexie                     |
| `QUEUE_RemoveAll` | Usuń wszystkie pasujące             |
| `QUEUE_IsEmpty`   | Czy pusta?                          |
| `QUEUE_IsFull`    | Czy pełna?                          |
| `QUEUE_Count`     | Ile elementów                       |
| `QUEUE_Clear`     | Wyczyść                             |