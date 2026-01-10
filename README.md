# Jeladó: 
- 1db fizikális gomb, hosszan bekapcsolás, röviden nyomva tárázás.
- LED indikátor, bekapcsolt.(Piros: Nincs kapcsolat a vevővel, Kék:Kapcsolodva a vevőhöz, Zöld:Adatok küldése)
- Bekapcsolás után tárázás. (Tárázásra a vevő figyelmeztet)
- Bottartó ráhelyezése
- A 0-tól való eltérés figyelése +-5g tűrés
- Eltérés (bot rá helyezése) >= 5g esetén jeladás, vagy folyamatos jeladás és az eltérést a vevő figyeli.
- Jeladáskor esetleg az akku töltöttségi szint jelének küldése.
- Viszaállás bot levétele, újabb jeladás 

# Vevő:
- Kijelző
- Be ki kapcsoló gomb, funkcióválasztó gomb, start gomb, 2 db hal (számláló) gomb ami plusz és mínusz kapcsolási funkciókat is ellát.
- A számláló működhet stopper és időzítőként is.
- Időzítő esetén, eltérés jel fogadásakor a beállított értéktől számol visszafelé, a nulla elérésekor a kijelző villog, vagy és csipogás hallatszik.
- Stopper esetén eltérés jel fogadása(bot rá helyezése a tartóra) esetén a vevő számlálója indul.
- Visszaszámllás jel esetén ( bot levétele a tartóról) a számláló megáll és az aktuális értéket mutatja, + a háttérben a valós idő tárolása ha valamelyik halszámláló gombot megnyomjuk).
- Újabb eltérés jel esetén a  a stopper számláló újraindul, az időzítő a  beállított értéktől számol visszafelé.
- Lehessen a jeladótol függetlenül is használni a stopper, időzítő, számláló funkciókat.

# "DRAM segment data does not fit."
Ha nem fér bele a DRAM-ba fordításkor akkor túl nagy a rajzolás Buffer mérete
A kód beillesztés ablakot az alt_gr+7 bill. kombinációval lehet készíteni.
```
 // Draw buffer for LVGL / TFT_eSPI display
//#define DRAW_BUF_SIZE (LVGL_CYD_TFT_WIDTH * LVGL_CYD_TFT_HEIGHT / 10 * (LV_COLOR_DEPTH / 8)) // Ez az alap beállítás.
#define DRAW_BUF_SIZE (LVGL_CYD_TFT_WIDTH * LVGL_CYD_TFT_HEIGHT / 15) // Ezzel a beállítással elindul, csak kicsit lassabb a renderelés. Alapból 10-es osztóval szépen megy csak akkor PSRAM kell hogy legyen a hardverben.
uint32_t draw_buf[DRAW_BUF_SIZE / 4];
```
