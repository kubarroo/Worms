# Weryfikacja cyklu życia

## Zakres automatyczny

Testy w `ApplicationTests/TestGameLifetime.cpp` używają sterowników SDL `dummy` i renderera `software`. Weryfikują logikę i zasoby bez okna interaktywnej rozgrywki.

| Scenariusz | Weryfikacja |
| --- | --- |
| Usunięcie robaka | Wybór następcy, destrukcja właściciela i zwolnienie jego encji oraz ciała |
| Śmierć ostatniej drużyny | Brak aktywnego robaka, usunięcie subskrypcji, zakończenie efektu śmierci i powrót liczników do stanu początkowego |
| Eksplozja pocisku | Utworzenie efektu i zlecenia usunięcia, usunięcie pocisku przez pętlę gry, brak pozostawionego ciała i subskrypcji |
| Zakończenie cząstek | Przejście przez kolejki dodawania i usuwania, zwolnienie encji systemu i cząstek |
| Deformacja terenu | Spadek liczby nieprzezroczystych pikseli, działająca tekstura zastępcza i stabilna liczba encji, ciał oraz subskrypcji |
| Powtórzone zlecenie usunięcia | Jedno sprzątanie i jedna destrukcja obiektu |
| Callbacki kolizji | Usunięcie własnej subskrypcji bez usunięcia innych słuchaczy, zgłoszenie wyjątku po kroku fizyki |
| Wielokrotny start i zamknięcie | Puste kontenery i rejestr subskrypcji, brak zasobów aplikacji, zamknięte SDL, audio i ImGui |
| Częściowa inicjalizacja | Sprzątanie po błędzie i możliwość powtórnego `CleanUp()` |

Powtarzalne cykle porównują dostępne sloty encji, ciała Box2D, subskrypcje oraz kolejki obiektów. Dodatkowo testy sprawdzają posiadane tekstury systemu cząstek i broni oraz teksturę i powierzchnię mapy przed i po sprzątaniu. Są to kontrole wybranych właścicieli, nie globalny licznik wszystkich alokacji SDL ani dowód braku wycieków.

## Uruchomienie

Po kompilacji `worms_tests`:

```powershell
ctest --test-dir build/debug --output-on-failure
```

Wielokrotne uruchomienie wszystkich testów w jednym procesie pomaga wykrywać stan pozostawiony przez wcześniejsze testy. Katalog roboczy musi zawierać zasoby gry:

```powershell
Push-Location Worms
try {
    & ../build/debug/ApplicationTests/worms_tests.exe --gtest_repeat=10 --gtest_shuffle --gtest_random_seed=12345
} finally {
    Pop-Location
}
```

## Diagnostyka pamięci

`TestMain.cpp` umożliwia diagnostykę sterty MSVC Debug przez `WORMS_CRT_DIAGNOSTICS=1`. Sprawdza integralność sterty przed i po każdym teście oraz włącza raport wycieków CRT przy końcu procesu. Raporty trafiają do standardowego wyjścia błędów, bez modalnego okna.

```powershell
$previous = $env:WORMS_CRT_DIAGNOSTICS
$env:WORMS_CRT_DIAGNOSTICS = '1'
Push-Location Worms
try {
    & ../build/debug/ApplicationTests/worms_tests.exe --gtest_repeat=10 --gtest_shuffle --gtest_random_seed=12345
} finally {
    Pop-Location
    $env:WORMS_CRT_DIAGNOSTICS = $previous
}
```

- Nieprawidłowa sterta powoduje błąd asercji testowej. Raport wycieków przy końcu procesu wymaga osobnej analizy i sam nie zmienia kodu wyjścia programu na błąd.
- Zielony wynik testów nie zastępuje sprawdzenia raportu wycieków. Zachowane bufory singletonów i bibliotek trzeba odróżnić od zasobów, które powinny zostać zwolnione.
- CRT sprawdza alokacje obsługiwane przez dany runtime Debug. Nie obejmuje automatycznie całej pamięci bibliotek zewnętrznych, sterowników ani GPU.
- Test kamery inicjalizuje podsystem zegara w lokalnym zakresie i zamyka SDL po zniszczeniu obiektów. Po całym zestawie runner zgłasza pozostawione aktywne podsystemy, następnie wykonuje końcowe `SDL_Init(SDL_INIT_TIMER)` i `SDL_Quit()`. Pozwala to posprzątać dane zegara i wątku utworzone także przez API wywołane poza pełną inicjalizacją SDL, np. podczas testów błędów. Raport CRT pozostaje włączony.
- AddressSanitizer wymaga osobnej kompilacji z instrumentacją. Obecność jego DLL w instalacji MSVC nie oznacza, że zwykły build Debug wykrywa use-after-free. Ten etap nie włącza instrumentacji ASan.

## Kontrola interaktywna

Testy headless nie zastępują sprawdzenia normalnych sterowników audio i grafiki. W rozgrywce należy sprawdzić śmierć robaka, eliminację ostatniej drużyny, eksplozję po kontakcie, widoczną deformację terenu, zanik cząstek oraz zamknięcie okna podczas aktywnego pocisku i efektów.

## Stan wykonania

- Przed dodaniem testów tego etapu uruchomiono istniejący zestaw: **70/70 testów przeszło**.
- Dodano sześć testów cykli życia oraz kontrole wybranych właścicieli zasobów i stanu po sprzątaniu. Próba kompilacji nowych zmian została zatrzymana przez błąd MSVC `C1902` w sandboxie; kompilacja poza sandboxem nie została zatwierdzona.
- Dostarczony log `lifetime-crt.log` potwierdza 10 przebiegów po 76 testów bez błędów testowych. Raport końcowy CRT zawierał pięć bloków o łącznym rozmiarze 213 bajtów. Osobne uruchomienie testu kamery odtworzyło 85 bajtów stanu zegara SDL, a testy błędów inicjalizacji odtworzyły alokacje związane z danymi wątku i buforem błędów. Po zmianie sprzątania runnera diagnostykę należy powtórzyć; wcześniejszy raport nie potwierdza jeszcze braku wycieków.
- Wynik po ostatniej poprawce sprzątania SDL, końcowy raport CRT oraz kontrolę interaktywną należy odnotować oddzielnie po ich rzeczywistym wykonaniu. Poprzednie zielone testy nie zastępują ponownej diagnostyki.
