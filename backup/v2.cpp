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

//+------------------------------------------------------------------+
//| Inicjalizacja                                                    |
//+------------------------------------------------------------------+
int OnInit() {
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
      if(sparam == "BTN_NET_ZERO") {
         Global_CloseNetZero = !Global_CloseNetZero;
         UpdateButton("BTN_NET_ZERO", "Pyramid Net Zero", Global_CloseNetZero);
      }
      if(sparam == "BTN_EACH_ZERO") {
         Global_CloseEachZero = !Global_CloseEachZero;
         UpdateButton("BTN_EACH_ZERO", "Each position Net Zero", Global_CloseEachZero);
      }
      ChartRedraw();
   };
   // Wstaw to w OnChartEvent pod obsługą pozostałych przycisków
   if(sparam == "BTN_PANIC") {
       // 1. Natychmiastowe wywołanie Twojej funkcji
       CloseAll(); 

       // 2. Reset flag automatów (żeby po zamknięciu wszystkiego robot nie szukał zera)
       Global_CloseNetZero = false;
       Global_CloseEachZero = false;

       // 3. Wizualne wyciśnięcie przycisków i zmiana kolorów na szary (używając Twoich funkcji)
       ObjectSetInteger(0, "BTN_NET_ZERO", OBJPROP_STATE, false);
       ObjectSetInteger(0, "BTN_EACH_ZERO", OBJPROP_STATE, false);
       ObjectSetInteger(0, "BTN_PANIC", OBJPROP_STATE, false);

       UpdateButton("BTN_NET_ZERO", "Pyramid Net Zero", false);
       UpdateButton("BTN_EACH_ZERO", "Each position Net Zero", false);

       ChartRedraw();
       Print("PANIC: Procedura zamknięcia wykonana.");
   }
}

// --- Funkcje pomocnicze GUI ---

// --- Zmienione funkcje pomocnicze GUI ---

void CreateGUI() {
   // 1. Przyciski funkcyjne (standardowy odstęp)
   CreateButton("BTN_NET_ZERO", 180, 30, "Pyramid Net Zero", Global_CloseNetZero);
   CreateButton("BTN_EACH_ZERO", 180, 70, "Each position Net Zero", Global_CloseEachZero);

   // 2. SEPARATOR (Pusty obszar blokujący missclicki)
   // Umieszczony na starym miejscu przycisku Panic, wysokość 30px
   CreateSpacer("GUI_SPACER", 180, 110, 150, 30); 

   // 3. PRZYCISK PANIC (Przesunięty niżej dla bezpieczeństwa)
   CreateButton("BTN_PANIC", 180, 150, "CLOSE ALL", false); 
   ObjectSetInteger(0, "BTN_PANIC", OBJPROP_BGCOLOR, clrRed); 
   ObjectSetInteger(0, "BTN_PANIC", OBJPROP_BORDER_COLOR, clrRed);
}

void CreateSpacer(string name, int x, int y, int width, int height) {
    if(ObjectFind(0, name) < 0) {
        ObjectCreate(0, name, OBJ_BUTTON, 0, 0, 0);
        ObjectSetInteger(0, name, OBJPROP_XDISTANCE, x);
        ObjectSetInteger(0, name, OBJPROP_YDISTANCE, y);
        ObjectSetInteger(0, name, OBJPROP_XSIZE, width);
        ObjectSetInteger(0, name, OBJPROP_YSIZE, height);
        
        ObjectSetString(0, name, OBJPROP_TEXT, ""); 
        ObjectSetInteger(0, name, OBJPROP_BGCOLOR, C'30,30,30'); // Dopasuj do koloru tła panelu
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