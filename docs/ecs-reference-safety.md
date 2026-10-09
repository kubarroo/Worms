# Bezpieczne odwołania do encji i komponentów ECS

## Zakres

Zasady obowiązują w obecnym ECS i należy zachować je podczas migracji do EnTT. Uzupełniają [kontrakt własności i sprzątania](ownership-and-cleanup.md).

## Referencje do komponentów

- Wskaźnik, referencja i `std::reference_wrapper` do komponentu są pożyczonym, krótkotrwałym dostępem, a nie własnością komponentu.
- Nie przechowujemy ich w polach obiektów ani między klatkami. Długotrwały obserwator przechowuje uchwyt encji i pobiera komponent przy każdym użyciu.
- Usuwanie komponentów lub encji może przesunąć inne komponenty w magazynie. Po takiej operacji wcześniejsze odwołanie może wskazywać dane innej encji.
- Nie utrzymujemy odwołań przez wywołanie callbacku lub funkcji, która może usunąć encję albo komponent. Dotyczy to również `b2World::DestroyBody()`, które może wywołać callbacki końca kontaktu.
- Potrzebne dane wejściowe kopiujemy przed takim wywołaniem. Po jego zakończeniu ponownie sprawdzamy ważność encji i pobieramy komponent, do którego chcemy zapisać wynik.
- `Camera::X()` i `Camera::Y()` zwracają wartości, a `FocusPoint::GetPos()` zwraca kopię `Position`. Zmiany pozycji kamery wykonujemy przez jej metody, nie przez zachowaną referencję do komponentu.

## Uchwyty obserwatorów

- `EntityId` jest numerem slotu, nie trwałą tożsamością encji. Numer `0` jest poprawnym identyfikatorem, a zwolniony numer może zostać przydzielony ponownie.
- Długotrwałe odwołania do cudzych encji używają `EntityHandle`, uzyskanego przez `World::GetHandle()`. Brak celu reprezentujemy przez pusty `std::optional`, nie przez numer `0`.
- Przed użyciem uchwytu sprawdzamy `World::IsAlive(handle)`. Sprawdzenie obejmuje numer slotu, generację oraz właściciela uchwytu.
- Sama ważność encji nie gwarantuje obecności `Position` ani innego komponentu. Po sprawdzeniu uchwytu pobieramy wymagany komponent przez `TryGetComponent()`.
- Utrata encji lub wymaganego komponentu czyści cel. Obserwator nie przejmuje automatycznie nowej encji pod tym samym numerem ani nie wznawia śledzenia po ponownym dodaniu komponentu. Wymaga to jawnego ustawienia celu.
- Broń po utracie rodzica zeruje ładowanie i nie strzela ani nie renderuje paska ładowania. Ponowne ustawienie tego samego ważnego rodzica zachowuje ładowanie; zmiana rodzica je zeruje.
- Uchwyty i obiekty przechowujące pożyczony `World*` nie mogą przeżyć swojego świata. Muszą zostać odłączone lub zniszczone przed zniszczeniem `World`. Uchwyt nie przedłuża życia świata, a jego pole `owner` nie służy do dereferencji.
- Właściciele encji mogą nadal używać `EntityId` w ramach własnego cyklu życia. Ich encji nie usuwamy niezależnie od właściciela, pozostawiając go z nieaktualnym numerem.

## Callbacki cząstek i iteracja systemów

- Funkcje charakterystyki cząstek wywoływane przez `ParticleUpdater` obliczają i zwracają wartości. Nie usuwają bezpośrednio encji ani nie dodają lub usuwają komponentów podczas iteracji systemu.
- System podczas iteracji korzysta z referencji do komponentów oraz zbioru subskrybowanych encji. Bezpośrednia zmiana struktury ECS z callbacku może unieważnić oba rodzaje dostępu.
- Jeśli callback ma zlecać zmiany strukturalne, zapisuje polecenie do kolejki. Polecenia wykonujemy po zakończeniu iteracji, bez zachowanych odwołań do komponentów.
- Kolejka odroczonych zmian komponentów nie jest obecnie ogólnym mechanizmem ECS. Przed dodaniem takiego zachowania do callbacków należy ją zaimplementować i dodać testy, w tym usunięcie bieżącej encji i ponowne wykorzystanie jej numeru.

## Kontrola zmian

Przy zmianach dotyczących obserwatorów i callbacków sprawdzamy: przesuwanie komponentów, usunięcie celu, usunięcie wymaganego komponentu, ponowne przydzielenie numeru encji przed następną aktualizacją oraz sprzątanie obserwatorów przed zniszczeniem świata.
