//+------------------------------------------------------------------+
//|                                              PyramidManager.mq4  |
//|                         Zarządzanie piramidą i Break-Even (GUI)  |
//+------------------------------------------------------------------+
#property strict

// --- Parametry zewnętrzne
input int MagicNumber = 0;       // 0 pilnuje wszystkich pozycji (z palca)
input int ActivationPips = 2;    // Ile pipsów zysku musi być, żeby pilnować zera

// --- Zmienne globalne stanu
bool Global_CloseNetZero  = false;
bool Global_CloseEachZero = false;

bool Global_MoneySL_Active = false; // Czy tryb jest aktywny
double Global_MoneySL_Value = 0; // Kwota straty (domyślnie -100)

//+------------------------------------------------------------------+
//| Inicjalizacja                                                    |
//+------------------------------------------------------------------+
int OnInit() {
   // Wymuszamy usunięcie śmieciowych obiektów po nazwie
   ObjectDelete(0, "GUI_SPACER"); 
   ObjectDelete(0, "BTN_PANIC_SPACER"); // Na wypadek gdyby tak się nazywał
   
   CreateGUI();
   return(INIT_SUCCEEDED);
}

//+------------------------------------------------------------------+
//| Deinicjalizacja                                                  |
//+------------------------------------------------------------------+
void OnDeinit(const int reason) {
   ObjectsDeleteAll(0, "BTN_"); // Czyści przyciski z wykresu
}

//+------------------------------------------------------------------+
//| Główna pętla programu                                            |
//+------------------------------------------------------------------+
void OnTick() {
   double totalNetProfit = 0;
   int activeMarketPositions = 0;
   bool pendingToClear = false; // Flaga wyzwalająca czyszczenie zleceń oczekujących
   double pipsToPoints = (Digits == 3 || Digits == 5) ? Point * 10 : Point;

   // --- 1. SKANOWANIE POZYCJI RYNKOWYCH ---
   for(int i = OrdersTotal() - 1; i >= 0; i--) {
      if(OrderSelect(i, SELECT_BY_POS, MODE_TRADES)) {
         if(OrderSymbol() == _Symbol && (MagicNumber == 0 || OrderMagicNumber() == MagicNumber)) {
            
            int type = OrderType();

            if(type == OP_BUY || type == OP_SELL) {
               double profit = OrderProfit() + OrderSwap() + OrderCommission();
               totalNetProfit += profit;
               activeMarketPositions++;

               // Logika: Każda na Zero (Global_CloseEachZero)
               if(Global_CloseEachZero) {
                  double openPrice = OrderOpenPrice();
                  double currentPrice = (type == OP_BUY) ? Bid : Ask;
                  double pipsProfit = (type == OP_BUY) ? (currentPrice - openPrice) : (openPrice - currentPrice);
                  pipsProfit /= pipsToPoints;

                  // Jeśli warunki zamknięcia są spełnione
                  if(profit <= 0 && pipsProfit < ActivationPips && OrderProfit() != 0) { 
                     if(OrderClose(OrderTicket(), OrderLots(), OrderClosePrice(), 3, clrRed)) {
                        pendingToClear = true; // Ustaw flagę, jeśli zamknięcie pojedynczej pozycji powiodło się
                     }
                  }
               }
            }
         }
      }
   }

// --- LOGIKA MONEY STOP LOSS (POPRAWIONA) ---
   if(Global_MoneySL_Active) {
      // 1. Aktualizacja linii SL u brokera (fizyczne linie na wykresie)
      RefreshGlobalSL(); 

      // 2. Pobieramy limit z okienka
      Global_MoneySL_Value = StringToDouble(ObjectGetString(0, "EDT_MONEY_VAL", OBJPROP_TEXT));

      // 3. Twardy bezpiecznik (jeśli suma strat dobije do limitu)
      if(activeMarketPositions > 0 && totalNetProfit <= Global_MoneySL_Value) {
          Print("Money Stop Loss osiągnięty! Zamykanie wszystkiego.");
          CloseAll();
          DeleteAllPending();
          
          Global_MoneySL_Active = false;
          UpdateButton("BTN_MONEY_SL", "Money SL Mode", false);
          ObjectSetInteger(0, "BTN_MONEY_SL", OBJPROP_STATE, false);
      }
   }

   // --- 2. USUWANIE OCZEKUJĄCYCH (Trigger: Każda na Zero) ---
   if(pendingToClear) {
      Print("Logika 'Każda na Zero' wyzwolona. Usuwanie zleceń oczekujących...");
      DeleteAllPending();
   }

   // --- 3. LOGIKA: PIRAMIDA NET ZERO ---
   if(Global_CloseNetZero && activeMarketPositions >= 2 && totalNetProfit <= 0) {
      Print("Piramida Net Zero: Zamykanie wszystkiego.");
      CloseAll(); 
      DeleteAllPending(); // Czyścimy oczekujące również przy zamknięciu całej piramidy
      
      Global_CloseNetZero = false;
      UpdateButton("BTN_NET_ZERO", "Piramida Net Zero", false);
      ObjectSetInteger(0, "BTN_NET_ZERO", OBJPROP_STATE, false);
   }
   
   // --- DODATKOWE ZABEZPIECZENIE: CZYŚCI OCZEKUJĄCE JEŚLI NIE MA POZYCJI ---
   if(activeMarketPositions == 0 && (Global_MoneySL_Active || Global_CloseNetZero)) {
      int pendings = 0;
      for(int i = OrdersTotal()-1; i >= 0; i--) {
         if(OrderSelect(i, SELECT_BY_POS, MODE_TRADES)) {
            if(OrderSymbol() == _Symbol && (MagicNumber == 0 || OrderMagicNumber() == MagicNumber)) {
               if(OrderType() > 1) pendings++;
            }
         }
      }
      
      if(pendings > 0) {
         Print("Wykryto zlecenia oczekujące bez pozycji rynkowych. Czyszczenie...");
         DeleteAllPending();
      }
   }
}

// --- FUNKCJA POMOCNICZA: USUWANIE WSZYSTKICH OCZEKUJĄCYCH ---
void DeleteAllPending() {
   for(int i = OrdersTotal() - 1; i >= 0; i--) {
      if(OrderSelect(i, SELECT_BY_POS, MODE_TRADES)) {
         if(OrderSymbol() == _Symbol && (MagicNumber == 0 || OrderMagicNumber() == MagicNumber)) {
            int type = OrderType();
            // Typy > 1 to: OP_BUYLIMIT, OP_SELLLIMIT, OP_BUYSTOP, OP_SELLSTOP
            if(type > 1) { 
               if(!OrderDelete(OrderTicket())) {
                  Print("Błąd usuwania zlecenia #", OrderTicket(), " Error: ", GetLastError());
               }
            }
         }
      }
   }
}

//+------------------------------------------------------------------+
//| Obsługa kliknięć w przyciski                                     |
//+------------------------------------------------------------------+
void OnChartEvent(const int id, const long &lparam, const double &dparam, const string &sparam) {
   if(id == CHARTEVENT_OBJECT_CLICK) {
      // 1. Obsługa Net Zero
      if(sparam == "BTN_NET_ZERO") {
         Global_CloseNetZero = !Global_CloseNetZero;
         UpdateButton("BTN_NET_ZERO", "Pyramid Net Zero", Global_CloseNetZero);
      }
      
      // 2. Obsługa Each Zero
      if(sparam == "BTN_EACH_ZERO") {
         Global_CloseEachZero = !Global_CloseEachZero;
         UpdateButton("BTN_EACH_ZERO", "Each position Net Zero", Global_CloseEachZero);
      }

      // 3. NOWE: Obsługa Money SL Mode (TEGO BRAKOWAŁO)
      if(sparam == "BTN_MONEY_SL") {
         Global_MoneySL_Active = !Global_MoneySL_Active;
         UpdateButton("BTN_MONEY_SL", "Money SL Mode", Global_MoneySL_Active);
      }
      
       // --- NOWOŚĆ: Przycisk usuwania wszystkich SL ---
      if(sparam == "BTN_REMOVE_SL") {
          // Wyłączamy automat Money SL, żeby nie nadpisał zmian
          Global_MoneySL_Active = false;
          UpdateButton("BTN_MONEY_SL", "Money SL Mode", false);
          ObjectSetInteger(0, "BTN_MONEY_SL", OBJPROP_STATE, false);

          RemoveAllStopLosses(); // Wywołanie funkcji czyszczącej
          
          ObjectSetInteger(0, "BTN_REMOVE_SL", OBJPROP_STATE, false);
      }
      // -----------------------------------------------    
      

      // 4. Panic Button
      if(sparam == "BTN_PANIC") {
         CloseAll(); 
         DeleteAllPending();

         Global_CloseNetZero = false;
         Global_CloseEachZero = false;
         Global_MoneySL_Active = false;

         ObjectSetInteger(0, "BTN_NET_ZERO", OBJPROP_STATE, false);
         ObjectSetInteger(0, "BTN_EACH_ZERO", OBJPROP_STATE, false);
         ObjectSetInteger(0, "BTN_MONEY_SL", OBJPROP_STATE, false);
         ObjectSetInteger(0, "BTN_PANIC", OBJPROP_STATE, false);

         UpdateButton("BTN_NET_ZERO", "Pyramid Net Zero", false);
         UpdateButton("BTN_EACH_ZERO", "Each position Net Zero", false);
         UpdateButton("BTN_MONEY_SL", "Money SL Mode", false);
      }
      
      ChartRedraw();
   }
}

// --- Funkcje pomocnicze GUI ---

// --- Zmienione funkcje pomocnicze GUI ---

void CreateGUI() {
   // 1. Grupa Net Zero
   CreateButton("BTN_NET_ZERO", 180, 30, "Pyramid Net Zero", Global_CloseNetZero);
   CreateButton("BTN_EACH_ZERO", 180, 70, "Each position Net Zero", Global_CloseEachZero);

   // 2. Money SL Mode
   CreateButton("BTN_MONEY_SL", 180, 110, "Money SL Mode", Global_MoneySL_Active);
   
   // 3. Okienko z kwotą (białe)
   ObjectDelete(0, "EDT_MONEY_VAL"); // Kasujemy stary śmieć z lewej strony
   if(ObjectFind(0, "EDT_MONEY_VAL") < 0) {
      ObjectCreate(0, "EDT_MONEY_VAL", OBJ_EDIT, 0, 0, 0);
      ObjectSetInteger(0, "EDT_MONEY_VAL", OBJPROP_CORNER, CORNER_RIGHT_UPPER);
      ObjectSetInteger(0, "EDT_MONEY_VAL", OBJPROP_XDISTANCE, 170);
      ObjectSetInteger(0, "EDT_MONEY_VAL", OBJPROP_YDISTANCE, 148); 
      ObjectSetInteger(0, "EDT_MONEY_VAL", OBJPROP_XSIZE, 150);
      ObjectSetInteger(0, "EDT_MONEY_VAL", OBJPROP_YSIZE, 25);
      ObjectSetString(0, "EDT_MONEY_VAL", OBJPROP_TEXT, "-10.00");
      ObjectSetInteger(0, "EDT_MONEY_VAL", OBJPROP_ALIGN, ALIGN_CENTER);
      ObjectSetInteger(0, "EDT_MONEY_VAL", OBJPROP_BGCOLOR, clrWhite);
      ObjectSetInteger(0, "EDT_MONEY_VAL", OBJPROP_COLOR, clrBlack);
   }

   // Wewnątrz void CreateGUI()
   CreateButton("BTN_REMOVE_SL", 180, 185, "REMOVE ALL SL", false);
   ObjectSetInteger(0, "BTN_REMOVE_SL", OBJPROP_BGCOLOR, clrDarkOrange); // Inny kolor dla odróżnienia
   
   // 4. Czerwony przycisk PANIC (bezpośrednio pod okienkiem, bez separatorów)
   CreateButton("BTN_PANIC", 180, 230, "CLOSE ALL", false); 
   ObjectSetInteger(0, "BTN_PANIC", OBJPROP_BGCOLOR, clrRed);
}

void CreateSpacer(string name, int x, int y, int width, int height) {
    if(ObjectFind(0, name) < 0) {
        ObjectCreate(0, name, OBJ_BUTTON, 0, 0, 0);
        ObjectSetInteger(0, name, OBJPROP_CORNER, CORNER_RIGHT_UPPER); // Musi być ten sam narożnik co przyciski!
        ObjectSetInteger(0, name, OBJPROP_XDISTANCE, x);
        ObjectSetInteger(0, name, OBJPROP_YDISTANCE, y);
        ObjectSetInteger(0, name, OBJPROP_XSIZE, width);
        ObjectSetInteger(0, name, OBJPROP_YSIZE, height);
        
        ObjectSetString(0, name, OBJPROP_TEXT, ""); 
        ObjectSetInteger(0, name, OBJPROP_BGCOLOR, clrBlack); 
        ObjectSetInteger(0, name, OBJPROP_BORDER_COLOR, clrNONE);
        ObjectSetInteger(0, name, OBJPROP_STATE, false);
        ObjectSetInteger(0, name, OBJPROP_SELECTABLE, false);
    }
}

void CreateButton(string name, int x, int y, string text, bool state) {
   ObjectCreate(0, name, OBJ_BUTTON, 0, 0, 0);
   
   // Ustawiamy kotwicę w prawym górnym rogu
   ObjectSetInteger(0, name, OBJPROP_CORNER, CORNER_RIGHT_UPPER);
   
   ObjectSetInteger(0, name, OBJPROP_XDISTANCE, x);
   ObjectSetInteger(0, name, OBJPROP_YDISTANCE, y);
   ObjectSetInteger(0, name, OBJPROP_XSIZE, 170);
   ObjectSetInteger(0, name, OBJPROP_YSIZE, 35);
   ObjectSetString(0, name, OBJPROP_TEXT, text);
   ObjectSetInteger(0, name, OBJPROP_FONTSIZE, 10);
   ObjectSetInteger(0, name, OBJPROP_SELECTABLE, false); // Żebyś nie przesunął ich myszką przez pomyłkę
   
   UpdateButton(name, text, state);
}

void UpdateButton(string name, string text, bool state) {
   ObjectSetInteger(0, name, OBJPROP_BGCOLOR, state ? clrDodgerBlue : clrGray);
   ObjectSetInteger(0, name, OBJPROP_COLOR, clrWhite);
   ObjectSetInteger(0, name, OBJPROP_STATE, false); 
}

void CloseAll() {
   for(int i = OrdersTotal() - 1; i >= 0; i--) {
      if(OrderSelect(i, SELECT_BY_POS, MODE_TRADES)) {
         if(OrderSymbol() == _Symbol && (MagicNumber == 0 || OrderMagicNumber() == MagicNumber)) {
            
            int type    = OrderType();
            int ticket  = OrderTicket();
            double lots = OrderLots();
            
            bool res = false;

            switch(type) {
               // --- Pozycje rynkowe ---
               case OP_BUY:  res = OrderClose(ticket, lots, Bid, 3, clrOrange); break;
               case OP_SELL: res = OrderClose(ticket, lots, Ask, 3, clrOrange); break;

               // --- Zlecenia oczekujące ---
               case OP_BUYLIMIT:
               case OP_SELLLIMIT:
               case OP_BUYSTOP:
               case OP_SELLSTOP: 
                  res = OrderDelete(ticket, clrPink); break;
            }
            
            if(!res) {
               Print("Błąd operacji na ticket #", ticket, " Error: ", GetLastError());
            }
         }
      }
   }
}


void RefreshGlobalSL() {
   if(!Global_MoneySL_Active) return;

   double targetLoss = StringToDouble(ObjectGetString(0, "EDT_MONEY_VAL", OBJPROP_TEXT));
   if(targetLoss >= 0) return; 

   double totalLots = 0;
   double weightedOpenPrice = 0;
   int type = -1;
   int count = 0;

   for(int i = OrdersTotal() - 1; i >= 0; i--) {
      if(OrderSelect(i, SELECT_BY_POS, MODE_TRADES)) {
         if(OrderSymbol() == _Symbol && (MagicNumber == 0 || OrderMagicNumber() == MagicNumber)) {
            if(OrderType() <= 1) {
               totalLots += OrderLots();
               weightedOpenPrice += OrderOpenPrice() * OrderLots();
               type = OrderType();
               count++;
            }
         }
      }
   }

   if(count == 0 || totalLots == 0) return;

   double avgOpenPrice = weightedOpenPrice / totalLots;

   // --- KLUCZOWA POPRAWKA PRZELICZNIKA ---
   // Obliczamy ile faktycznie kosztuje zmiana o 1.0 (pełny punkt ceny) dla 1 lota
   // Na parach walutowych zmiana z 1.1000 na 1.2000 przy 1 lot to 10 000 jednostek.
   double pointValueInMoney = MarketInfo(_Symbol, MODE_TICKVALUE) / MarketInfo(_Symbol, MODE_TICKSIZE);
   
   // Obliczamy dystans w cenie: Dystans = Kwota / (Loty * Wartość Punktu)
   double priceDistance = MathAbs(targetLoss) / (totalLots * pointValueInMoney);
   
   double slPrice;
   if(type == OP_BUY) slPrice = avgOpenPrice - priceDistance;
   else slPrice = avgOpenPrice + priceDistance;

   slPrice = NormalizeDouble(slPrice, Digits);

   // --- MODYFIKACJA ---
   for(int i = OrdersTotal() - 1; i >= 0; i--) {
      if(OrderSelect(i, SELECT_BY_POS, MODE_TRADES)) {
         if(OrderSymbol() == _Symbol && (MagicNumber == 0 || OrderMagicNumber() == MagicNumber)) {
            if(MathAbs(OrderStopLoss() - slPrice) > Point) {
               OrderModify(OrderTicket(), OrderOpenPrice(), slPrice, OrderTakeProfit(), 0, clrOrange);
            }
         }
      }
   }
}




void RemoveAllStopLosses() {
   for(int i = OrdersTotal() - 1; i >= 0; i--) {
      if(OrderSelect(i, SELECT_BY_POS, MODE_TRADES)) {
         if(OrderSymbol() == _Symbol && (MagicNumber == 0 || OrderMagicNumber() == MagicNumber)) {
            if(OrderType() <= 1) { // Tylko pozycje BUY i SELL
               if(OrderStopLoss() != 0) {
                  if(!OrderModify(OrderTicket(), OrderOpenPrice(), 0, OrderTakeProfit(), 0, clrWhite)) {
                     Print("Błąd usuwania SL dla ticket #", OrderTicket(), " Error: ", GetLastError());
                  }
               }
            }
         }
      }
   }
   Print("Wszystkie Stop Lossy zostały usunięte.");
}