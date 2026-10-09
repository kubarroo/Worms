# Własność zasobów i cykl życia obiektów

## Status i zakres

Dokument definiuje proponowany kontrakt dla fazy 1: stabilizacji własności, sprzątania i zamykania aplikacji. Nie oznacza, że opisane zasady są już zaimplementowane.

Podstawą jest wcześniejszy przegląd projektu. Podczas tworzenia dokumentu narzędzie terminala nie pozwoliło ponownie odczytać plików, dlatego aktualny kod należy zweryfikować przed implementacją. Dokument nie obejmuje migracji do SDL3, EnTT, wprowadzenia Scene ani pełnego menedżera zasobów.

## Reguły własności

Zasady krótkotrwałego dostępu do komponentów, ważności uchwytów encji i ograniczeń callbacków opisuje dokument [Bezpieczne odwołania do encji i komponentów ECS](ecs-reference-safety.md).

- Każdy zasób ma jednego jawnego właściciela odpowiedzialnego za zwolnienie.
- Pole przechowywane przez wartość albo unique_ptr oznacza własność. Surowy wskaźnik lub referencja oznacza dostęp bez prawa do usuwania.
- Współdzielony dostęp do tekstury lub dźwięku nie wymaga automatycznie shared_ptr. Właściciel musi żyć dłużej niż wszyscy użytkownicy zasobu.
- Komponenty Sprite i RigidBody przechowują pożyczone uchwyty. Usunięcie komponentu samo w sobie nie zwalnia tekstury ani ciała Box2D.
- Kopiowanie właściciela zasobu jest zabronione, chyba że klasa definiuje poprawną semantykę kopii. Przenoszenie musi zachować adresy, od których zależą callbacki i userData, albo poprawić te odwołania.
- Wskaźniki zaczynają od nullptr, a identyfikatory od jawnego stanu nieważnego zgodnego z aktualnym ECS. Nie należy zakładać, że identyfikator 0 jest nieważny.

## Tabela własności

Tabela opisuje docelową odpowiedzialność w ramach obecnej struktury klas. Nie narzuca jeszcze konkretnej implementacji uchwytów RAII.

| Zasób / obiekt | Właściciel | Obserwatorzy / użytkownicy | Odpowiedzialność za zwolnienie |
| --- | --- | --- | --- |
| SDL_Window, SDL_Renderer | App | Game i obiekty renderujące | App; renderer przed oknem, po zwolnieniu wszystkich tekstur |
| Inicjalizacja SDL, audio i backendów ImGui | App | cała aplikacja | App; zamyka tylko poprawnie zainicjalizowane podsystemy |
| World ECS, menedżery ECS | App / World | GameObject, systemy i menedżery gry | World po odłączeniu wszystkich obiektów |
| Systemy i magazyny komponentów | odpowiednie menedżery ECS | World | menedżery; zweryfikować destruktory baz polimorficznych |
| b2World i debug draw | App | ColliderFactory, obiekty fizyki | App po usunięciu obiektów i odpięciu listenera |
| WormManager, WeaponManager, Music | Game | logika gry | Game; Music przed zamknięciem audio |
| WormTeam | WormManager | aktywna drużyna i logika tur | WormManager po odłączeniu robaków |
| Worm | WormTeam | aktywny robak, kamera, broń | WormTeam po Worm::CleanUp() |
| HealthBar robaka | Worm | renderowanie i obsługa obrażeń | Worm po odłączeniu encji paska |
| Tekstura pasków zdrowia drużyny | WormTeam | HealthBar należące do jej robaków | WormTeam po zniszczeniu wszystkich pasków |
| Map, Camera, Weapon, Projectile, ParticleSystem w aktywnej liście | GameObject::activeObjs | Game i menedżery | kontener po CleanUp() każdego zainicjalizowanego obiektu |
| Obiekty w objsToAdd | GameObject::objsToAdd | logika tworzenia obiektów | kontener; po aktywacji własność przechodzi do activeObjs |
| Wpisy objsToDelete | brak własności; kolejka poleceń | pętla gry | tylko zlecenie usunięcia; nie wykonuje niezależnego delete |
| WeaponImpl i dźwięki / tekstury konfiguracji broni | WeaponManager | Weapon i Projectile | WeaponManager po odłączeniu wszystkich użytkowników |
| Tekstura paska ładowania | Weapon | Weapon::Render() | Weapon przed zniszczeniem renderera |
| Tekstura sprite robaka, obecnie ładowana osobno | Worm | komponent Sprite robaka | Worm; HealthBar nie przejmuje jej własności |
| Tekstura efektu cząsteczkowego | ParticleSystem | Sprite jego cząsteczek | ParticleSystem po usunięciu encji cząsteczek |
| Dane obrazu terenu i jego aktualna tekstura | Map / jego wrapper PhysicTexture | generowanie terenu i Sprite mapy | Map poprzez jednego właściciela; sprawdzić powierzchnię i bufor pikseli |
| Wrapper Collider | Worm lub Projectile | logika danego obiektu | obiekt posiadający wrapper |
| Ciało i fixtures Box2D | b2World fizycznie przechowuje; obiekt gry odpowiada za wcześniejsze usunięcie | RigidBody, Collider i callbacki | CleanUp() obiektu wywołuje DestroyBody raz; fixture usuwa się wraz z ciałem |
| Dane wskazywane przez body / fixture userData | obiekt gry, który je udostępnia | Box2D i ContactManager | muszą istnieć aż do zakończenia DestroyBody; nie wskazują na tymczasowe argumenty |
| Encje robaka, sensora, paska, mapy, broni i pocisku | obiekt, który je utworzył | ECS, kamera i logika gry | CleanUp() twórcy usuwa wszystkie jego encje |
| Encja ParticleSystem i encje jego cząsteczek | ParticleSystem | systemy ECS | ParticleSystem usuwa zarówno cząsteczki, jak i własną encję |
| FocusPoint i jego encja | Camera / FocusPoint | śledzenie celu | Camera odkłada sprzątanie encji na czas, gdy ECS jeszcze istnieje |
| Mix_Chunk, Mix_Music | Sound, Music | odtwarzanie audio | wrapper po zatrzymaniu użycia zasobu, przed zamknięciem audio |
| Subskrypcja kolizji | obiekt rejestrujący callback | ContactManager przechowuje funkcję | obiekt wyrejestrowuje ją przed zniszczeniem stanu przechwyconego przez callback |
| noTargetEvent kamery | Camera przechowuje funkcję; WormManager odpowiada za ważność przechwyconego siebie | Camera::Update() | odpiąć przed zniszczeniem WormManager |

Singletony ContactManager i ColliderFactory nie posiadają obiektów gry ani świata fizyki. Ich pożyczone odwołania i rejestry trzeba wyczyścić przed zniszczeniem zależności. Sposób resetowania singletonów wymaga sprawdzenia aktualnego API.

## Kontrakt CleanUp()

### Cel i wywołujący

CleanUp() odczepia obiekt od działającego świata gry. Właściciel wywołuje je przed zniszczeniem zainicjalizowanego obiektu, zarówno podczas rozgrywki, jak i zamykania aplikacji.

Destruktor zwalnia lokalne zasoby posiadane przez obiekt. Nie polegamy na wywołaniu wirtualnego CleanUp() z destruktora GameObject: takie wywołanie nie wykona implementacji klasy pochodnej. App i GameObject muszą mieć wirtualne destruktory, jeżeli obiekty pochodne są usuwane przez wskaźnik do bazy.

### Wymagania

- CleanUp() jest idempotentne: kolejne wywołanie nie usuwa zasobów drugi raz.
- Działa po częściowej inicjalizacji, sprawdzając rzeczywiście pozyskane zasoby.
- Nie propaguje wyjątków podczas zamykania. Docelowo powinno spełniać kontrakt noexcept; przed dodaniem deklaracji trzeba zweryfikować wywoływane operacje.
- Po rozpoczęciu sprzątania obiekt nie wykonuje Update(), Render() ani nowych callbacków wymagających jego zasobów.
- Usuwa wszystkie encje utworzone przez obiekt, łącznie z pomocniczymi sensorami i efektami.
- Odłącza subskrypcje przed usunięciem stanu, do którego callbacki się odwołują.
- DestroyBody jest wykonywane poza krokiem symulacji, przy niezablokowanym świecie Box2D. Dane userData pozostają ważne przez całe wywołanie.
- Po zwolnieniu ciała i encji ich uchwyty są unieważniane. Samo wyzerowanie Sprite.texture nie zwalnia tekstury.
- Zwolnienie lokalnych tekstur może nastąpić w destruktorze, ale dopiero po usunięciu komponentów, które ich używają, i przed zniszczeniem renderera.

### Kolejność dla pojedynczego obiektu

1. Oznaczyć obiekt jako nieaktywny / sprzątany i uniemożliwić kolejne aktualizacje.
2. Unieważnić odwołania obserwatorów albo zagwarantować sprawdzenie ważności celu przed kolejnym użyciem.
3. Wyrejestrować callbacki przechwytujące obiekt oraz zatrzymać zewnętrzne użycie lokalnych zasobów.
4. Odłączyć posiadane obiekty pomocnicze, jeśli ich sprzątanie wymaga nadal istniejących encji rodzica.
5. Zniszczyć posiadane ciała fizyki, zachowując ważność userData i encji do końca operacji.
6. Usunąć pozostałe encje pomocnicze i encję główną; unieważnić uchwyty.
7. Zniszczyć obiekt przez jego właściciela. Destruktor zwalnia lokalne zasoby.

Dokładny porządek obiektów pomocniczych wynika z ich zależności. Nie wystarczy wywołać bazowego CleanUp(), jeżeli klasa tworzy dodatkowe encje, ciała lub subskrypcje.

### Kolejki i obserwatorzy

- Wielokrotne zgłoszenie tego samego obiektu do usunięcia skutkuje jednym sprzątaniem i jednym zniszczeniem.
- Przetwarzanie usunięcia odbywa się w bezpiecznym punkcie pętli, poza iteracją aktualizującą dany kontener i poza krokiem Box2D.
- Zamykanie obejmuje aktywne obiekty i obiekty oczekujące na dodanie. Te drugie mogły już pozyskać zasoby w konstruktorze.
- Przed usunięciem robaka lub drużyny poprawiane są wskaźniki aktywnego robaka i aktywnej drużyny.
- Cel kamery i rodzic broni stają się nieważne po usunięciu odpowiedniej encji. Recykling identyfikatora nie może przypadkowo przepiąć ich na nowy obiekt.
- Camera nie przechowuje trwale Position*: obecny magazyn komponentów może przenosić elementy przy usuwaniu. Komponent pobierany jest przez ważny identyfikator encji przy użyciu.

## Kolejność zamykania aplikacji

Zamykanie musi działać również po błędzie inicjalizacji. Poniższa kolejność jest proponowana dla obecnych zależności; wymaga jawnej koordynacji przez Game i App, ponieważ statyczne kontenery nie gwarantują poprawnego czasu zniszczenia.

1. Zatrzymać pętlę gry, aktualizacje, tworzenie obiektów i inicjowanie nowych efektów audio.
2. Odłączyć noTargetEvent oraz powiązania aktywnego robaka, drużyny, celu kamery i rodzica broni. Odłączyć callbacki kolizji obiektów przeznaczonych do usunięcia.
3. Wywołać CleanUp() wszystkich aktywnych oraz częściowo zainicjalizowanych oczekujących obiektów, gdy ECS, Box2D i menedżery zasobów nadal istnieją. Odłączyć robaki, paski zdrowia i FocusPoint.
4. Zniszczyć obiekty korzystające z zasobów konfiguracji broni, w tym pociski i Weapon. Opróżnić activeObjs, objsToAdd i objsToDelete; nie tworzyć nowych obiektów podczas sprzątania.
5. Zniszczyć WormManager i jego drużyny oraz WeaponManager. Paski zdrowia muszą zostać zniszczone przed teksturami drużyn. Zwolnić pozostałe tekstury i wrappery obiektów gry.
6. Zatrzymać odtwarzanie muzyki i kanałów, a potem zwolnić pozostałe Sound i Music. Zweryfikować sposób zatrzymania użycia zasobów z aktualną wersją SDL_mixer.
7. Wyczyścić ContactManager, odpiąć listener i debug draw od Box2D oraz zresetować pożyczone odwołanie ColliderFactory do b2World.
8. Zniszczyć światy ECS i Box2D oraz debug draw po zakończeniu wszystkich operacji obiektów gry.
9. Zamknąć backendy ImGui i kontekst, gdy renderer i okno jeszcze istnieją.
10. Zamknąć audio i podsystemy bibliotek pomocniczych odpowiednio do faktycznie wykonanej inicjalizacji.
11. Zniszczyć renderer, następnie okno, na końcu zamknąć SDL.

Normalne wyjście i obsługa błędu inicjalizacji korzystają z tego samego kontraktu. Zasoby pozyskane w konstruktorze, który rzuci wyjątek, muszą być zabezpieczone lokalnym RAII, ponieważ destruktor niedokonstruowanego obiektu nie zostanie wywołany.

## Miejsca do potwierdzenia przed implementacja

- Aktualna lista zasobów i destruktory Sound, Music, PhysicTexture oraz baz ECS.
- Wszystkie miejsca tworzenia i wymiany tekstur, w tym przebudowa terenu i nieudane ładowanie.
- Czas życia zasobów WeaponManager pożyczanych przez Projectile.
- Konstrukcja i inicjalizacja FocusPoint oraz sprzątanie jego encji.
- Przypadki callbacków wykonywanych podczas DestroyBody i zasady modyfikacji rejestru listenerów podczas wywołania.
- Wszystkie adresy przekazywane do userData; szczególnie argumenty tymczasowe oraz możliwość przeniesienia obiektu.
- Możliwość zidentyfikowania nieważnej encji w obecnym ECS oraz ochrona przed recyklingiem identyfikatorów.

## Kryteria przyjęcia kontraktu

- Dla każdego zasobu wskazany jest właściciel i miejsce zwolnienia; obserwatorzy nie wykonują delete ani zwolnienia pożyczonego uchwytu.
- Sprzątanie obejmuje usunięcie podczas gry, normalne zamknięcie i błąd częściowej inicjalizacji.
- Powtórne sprzątanie i wielokrotne zgłoszenie usunięcia nie powodują podwójnego zwolnienia.
- Przed zniszczeniem zależności nie pozostają obiekty, komponenty ani callbacki, które z nich korzystają.
- Powtarzane cykle tworzenia i usuwania wracają do oczekiwanej liczby encji, ciał, subskrypcji i zasobów. Ocena pamięci uwzględnia pule i cache bibliotek, a nie wymaga identycznego zużycia pamięci procesu.
