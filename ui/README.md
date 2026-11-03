# Search Lab — ndërfaqja lokale

UI për Latin Square, Sudoku dhe Social Golfers. Algoritmet C++ nuk ndryshohen. Serveri lokal ekzekuton programet nga `../bin`, lexon daljen dhe e kontrollon përsëri rezultatin para paraqitjes.

## Nisja në PowerShell

```powershell
cd "C:\Users\kaltr\Desktop\Ushtrime_Algoritme\PROJEKT_AI\ui"
npm start
```

Hap **http://127.0.0.1:8787/** në shfletues. Mbaje terminalin hapur. Për të ndalur serverin shtyp `Ctrl+C` në atë terminal. Nëse porti është zënë, ndal instancën e mëparshme ose zgjidh një port tjetër me `$env:PORT = '8788'` para nisjes.

Duhet Node.js (i testuar me v20). Për përdorimin e UI-së së kompiluar **nuk nevojitet npm install**: serveri përdor vetëm modulet standarde të Node dhe `public/assets/app.js` është përfshirë.

Programet `latin_square.exe`, `sudoku.exe`, `social_golfers.exe` duhet të jenë në `PROJEKT_AI/bin`. Rruga përcaktohet automatikisht nga vendndodhja e UI-së; për një projekt tjetër mund të caktohet `AI_PROJECT_ROOT`.

## Përdorimi

- Latin Square: zgjidh madhësinë n=1..8 dhe IDDFS ose backtracking. Për n=7,8 IDDFS mund të arrijë kufirin e nyjeve dhe shfaqet UNKNOWN.
- Sudoku: zgjidh shembullin ose ndrysho numrat drejtpërdrejt në tabelë. `Rikthe hyrjen` largon rezultatin dhe mundëson redaktimin përsëri. Etiketat e vështirësisë janë demonstrative.
- Golfers: zgjidh grupet, lojtarët/grup dhe javët. UI deklaron kur programi ekzistues kalon te modeli i specializuar për 8×4 dhe 8..10 javë. Eksperimenti i veçantë me DFS të përgjithshëm lexohet vetëm për status; UI nuk e ndal apo ndryshon.
- Shfaqen koha reale e kërkimit e raportuar nga C++, vizitat, metrikat përkatëse dhe verifikimi i rezultatit.
- `Luaj` dhe `Rreshti tjetër` shfaqin gradualisht **zgjidhjen përfundimtare**, jo hapat realë të BFS/DFS/IDDFS. Kjo thuhet edhe në ndërfaqe.
- Eksperimentet e nisura nga UI kanë kufi kohor të zgjedhshëm 5..120 sekonda. `Ndalo këtë eksperiment` ndalon vetëm procesin e krijuar nga ai eksperiment; nuk prek kërkimin ekzistues në sfond.
- Dalja origjinale e programit mund të hapet dhe rezultati mund të shkarkohet si JSON.

## Organizimi

```text
ui/
  src/app.ts             # Ndërveprimet dhe paraqitja
  public/index.html      # Struktura e faqes
  public/styles.css      # Pamja responsive
  public/assets/app.js   # TypeScript i kompiluar, gati për përdorim
  server.mjs             # Serveri HTTP dhe ekzekutimi lokal i programeve
  solver-adapter.mjs     # Validimi i parametrave, leximi dhe kontrolli i daljes
  test/server.test.mjs   # Testet e adapterit dhe lidhjes me C++
```

## Ndryshimi i TypeScript dhe testet

```powershell
npm ci
npm run build
npm test
npm start
```

Pas ndryshimeve bëj refresh në shfletues. Serveri duhet rinisur pas ndryshimeve në `server.mjs` ose `solver-adapter.mjs`.

Testet mbulojnë hyrjet e pavlefshme, validatorët e pavarur, zgjidhjet reale të të tre programeve, dallimin e modelit të specializuar, limitin UNKNOWN, UNSAT dhe anulimin e një procesi të UI-së. Nuk kërkohet ndryshim ose rikompilim i C++ për këtë ndërfaqe.

UI është vetëm lokale dhe nuk është publikuar në internet. Për të shmangur ekzekutim të komandave të lira, programet dhe argumentet përzgjidhen nga një listë e kufizuar dhe nuk përdoret shell për nisjen e tyre.
