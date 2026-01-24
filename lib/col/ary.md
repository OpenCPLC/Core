# ary

Uniwersalny array/ring buffer. Wszystko O(1).

## Tryby
```c
ary_new(nums, int, 10);           // array - jak pełny to push zwraca false
ary_ring(samples, uint16_t, 64);  // ring - jak pełny to nadpisuje najstarszy
```

## Przykład - stos
```c
ary_new(stack, int, 8);

int x = 10;
ary_push(&stack, &x);
x = 20;
ary_push(&stack, &x);
x = 30;
ary_push(&stack, &x);

int out;
while(!ary_empty(&stack)) {
  ary_pop(&stack, &out);
  LOG_Info("Pop: %d", out);  // 30, 20, 10
}
```

## Przykład - kolejka FIFO
```c
ary_new(fifo, int, 8);

int x = 10;
ary_push(&fifo, &x);
x = 20;
ary_push(&fifo, &x);

int out;
while(!ary_empty(&fifo)) {
  ary_shift(&fifo, &out);    // shift = zdejmij z przodu
  LOG_Info("Out: %d", out);  // 10, 20
}
```

## Przykład - ring buffer (np. ostatnie N próbek ADC)
```c
ary_ring(adc_buf, uint16_t, 4);  // trzyma ostatnie 4

for(int i = 0; i < 10; i++) {
  uint16_t sample = ADC_Read();
  ary_push(&adc_buf, &sample);  // jak pełny to wyrzuca najstarszy
}

// zawsze masz ostatnie 4 próbki
ary_for(i, &adc_buf) {
  uint16_t *val = ary_get(&adc_buf, i);
  LOG_Info("Sample[%d]: %d", i, *val);
}
```

## Przykład - z wartościami początkowymi
```c
ary_init(pins, uint8_t, 8, {PA0, PA1, PA2, PB0});

ary_for(i, &pins) {
  GPIO_Init(*ary_get(&pins, i), OUTPUT);
}
```

## API

| Funkcja          | Opis                |
| ---------------- | ------------------- |
| `ary_push`       | Dodaj na koniec     |
| `ary_pop`        | Zdejmij z końca     |
| `ary_shift`      | Zdejmij z początku  |
| `ary_unshift`    | Dodaj na początek   |
| `ary_get(i)`     | Wskaźnik na element |
| `ary_set(i)`     | Nadpisz element     |
| `ary_insert(i)`  | Wstaw w środek      |
| `ary_remove(i)`  | Usuń ze środka      |
| `ary_swap(i,j)`  | Zamień miejscami    |
| `ary_peek`       | Podglądnij ostatni  |
| `ary_peek_first` | Podglądnij pierwszy |
| `ary_clear`      | Wyczyść             |
| `ary_empty`      | Czy pusty?          |
| `ary_full`       | Czy pełny?          |
| `ary_free`       | Ile wolnych slotów  |