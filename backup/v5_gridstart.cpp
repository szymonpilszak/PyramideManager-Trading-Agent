//+------------------------------------------------------------------+
//|                                              PyramidManager.mq4  |
//|                    Zarządzanie piramidą i Break-Even (GUI)       |
//+------------------------------------------------------------------+
#property strict

// --- Parametry zewnętrzne
input int MagicNumber = 0;       
input int ActivationPips = 2;    

// --- Zmienne globalne stanu
bool Global_CloseNetZero   = false;
bool Global_CloseEachZero  = false;
bool Global_MoneySL_Active = false; 
double Global_MoneySL_Value = 0; 
datetime lastModificationTime = 0; 
bool Global_GridPanel_Open = false;
int  Global_Grid_Direction = 0; // 0-BuyStop, 1-SellStop

//+------------------------------------------------------------------+
//| Inicjalizacja                                                    |
//+------------------------------------------------------------------+
int OnInit() {
   ObjectDelete(0, "GUI_SPACER"); 
   CreateGUI();
   return(INIT_SUCCEEDED);
}

void OnDeinit(const int reason) {
   ObjectsDeleteAll(0, "BTN_"); 
   ObjectDelete(0, "EDT_MONEY_VAL");
}

//+------------------------------------------------------------------+
//| Główna pętla programu                                            |
//+------------------------------------------------------------------+
void OnTick() {
   double totalNetProfit = 0;
   int activeMarketPositions = 0;
   bool pendingToClear = false; 
   double pipsToPoints = (Digits == 3 || Digits == 5) ? Point * 10 : Point;

   // --- 1. SKANOWANIE POZYCJI ---
   for(int i = OrdersTotal() - 1; i >= 0; i--) {
      if(OrderSelect(i, SELECT_BY_POS, MODE_TRADES)) {
         if(OrderSymbol() == _Symbol && (MagicNumber == 0 || OrderMagicNumber() == MagicNumber)) {
            int type = OrderType();
            if(type == OP_BUY || type == OP_SELL) {
               double profit = OrderProfit() + OrderSwap() + OrderCommission();
               totalNetProfit += profit;
               activeMarketPositions++;

               if(Global_CloseEachZero) {
                  double openPrice = OrderOpenPrice();
                  double currentPrice = (type == OP_BUY) ? Bid : Ask;
                  double pipsProfit = (type == OP_BUY) ? (currentPrice - openPrice) : (openPrice - currentPrice);
                  pipsProfit /= pipsToPoints;

                  // Each Position Net Zero Logic
                  if(profit <= 0 && pipsProfit < ActivationPips && OrderProfit() != 0) { 
                     if(OrderClose(OrderTicket(), OrderLots(), OrderClosePrice(), 3, clrRed)) {
                        pendingToClear = true;
                     }
                  }
               }
            }
         }
      }
   }

   // --- 2. LOGIKA MONEY STOP LOSS ---
   if(Global_MoneySL_Active && activeMarketPositions > 0) {
      if(TimeCurrent() > lastModificationTime) {
          RefreshGlobalSL();
          lastModificationTime = TimeCurrent();
      }

      Global_MoneySL_Value = StringToDouble(ObjectGetString(0, "EDT_MONEY_VAL", OBJPROP_TEXT));

      if(totalNetProfit <= Global_MoneySL_Value) {
          Print("Money Stop Loss osiągnięty! Zamykanie.");
          CloseAll();
          DeleteAllPending();
          ResetAllModes();
      }
   }

   if(pendingToClear) DeleteAllPending();

   // --- 3. PIRAMIDA NET ZERO ---
   if(Global_CloseNetZero && activeMarketPositions >= 2 && totalNetProfit <= 0) {
      CloseAll(); 
      DeleteAllPending();
      ResetAllModes();
   }
   
   if(activeMarketPositions == 0 && (Global_MoneySL_Active || Global_CloseNetZero || Global_CloseEachZero)) {
       DeleteAllPending();
   }
}

//+------------------------------------------------------------------+
//| Funkcje Zarządzania SL                                           |
//+------------------------------------------------------------------+
void RefreshGlobalSL() {
   if(!Global_MoneySL_Active) return;

   double targetMoney = StringToDouble(ObjectGetString(0, "EDT_MONEY_VAL", OBJPROP_TEXT));
   double totalLots = 0, weightedOpenPrice = 0;
   int type = -1, count = 0;

   // 1. Skanowanie pozycji
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
   double tickVal = MarketInfo(_Symbol, MODE_TICKVALUE);
   double tickSz  = MarketInfo(_Symbol, MODE_TICKSIZE);
   double stopLevel = MarketInfo(_Symbol, MODE_STOPLEVEL) * Point; // Minimalny dystans od ceny
   
   if(tickVal <= 0 || tickSz <= 0) return;
   
   double pointValueInMoney = tickVal / tickSz;
   double priceDistance = targetMoney / (totalLots * pointValueInMoney);
   
   double slPrice = (type == OP_BUY) ? avgOpenPrice + priceDistance : avgOpenPrice - priceDistance;
   slPrice = NormalizeDouble(slPrice, Digits);

   // 2. Sprawdzenie bezpiecznego dystansu (StopLevel)
   bool canModify = false;
   if(type == OP_BUY) {
      if(Bid - slPrice > stopLevel) canModify = true; 
   } else {
      if(slPrice - Ask > stopLevel) canModify = true;
   }

   if(!canModify) return; // Jeśli cena jest za blisko planowanego SL/TP, nie rób nic (czekaj na ruch)

   // 3. Wykonanie modyfikacji
   for(int i = OrdersTotal() - 1; i >= 0; i--) {
      if(OrderSelect(i, SELECT_BY_POS, MODE_TRADES)) {
         if(OrderSymbol() == _Symbol && (MagicNumber == 0 || OrderMagicNumber() == MagicNumber)) {
            // Modyfikuj tylko jeśli różnica jest większa niż 1 punkt (oszczędność łącza)
            if(MathAbs(OrderStopLoss() - slPrice) > Point) {
               OrderModify(OrderTicket(), OrderOpenPrice(), slPrice, OrderTakeProfit(), 0, clrOrange);
            }
         }
      }
   }
}

void RemoveAllStopLosses() {
    for(int i = OrdersTotal() - 1; i >= 0; i--) {
        if(OrderSelect(i, SELECT_BY_POS) && OrderSymbol() == Symbol() && (MagicNumber == 0 || OrderMagicNumber() == MagicNumber)) {
            if(OrderStopLoss() != 0) {
                RefreshRates();
                OrderModify(OrderTicket(), OrderOpenPrice(), 0, OrderTakeProfit(), 0, clrNONE);
            }
        }
    }
}

//+------------------------------------------------------------------+
//| Obsługa GUI                                                      |
//+------------------------------------------------------------------+
void OnChartEvent(const int id, const long &lparam, const double &dparam, const string &sparam) {
   if(id == CHARTEVENT_OBJECT_CLICK) {
      
      // --- 1. Obsługa głównych trybów ---
      if(sparam == "BTN_NET_ZERO") {
          Global_CloseNetZero = !Global_CloseNetZero;
          UpdateButton("BTN_NET_ZERO", "Pyramid Net Zero", Global_CloseNetZero);
      }
      
      if(sparam == "BTN_EACH_ZERO") {
          Global_CloseEachZero = !Global_CloseEachZero;
          UpdateButton("BTN_EACH_ZERO", "Each position Net Zero", Global_CloseEachZero);
      }
      
      if(sparam == "BTN_MONEY_SL") {
          Global_MoneySL_Active = !Global_MoneySL_Active;
          UpdateButton("BTN_MONEY_SL", "Money SL Mode", Global_MoneySL_Active);
      }

      // --- 2. Panel Grid: Otwórz / Zamknij ---
      if(sparam == "BTN_GRID_TOGGLE") {
          Global_GridPanel_Open = !Global_GridPanel_Open;
          if(Global_GridPanel_Open) 
             CreateGridSubPanel();
          else 
             HideGridSubPanel(); // Czyści wszystko, łącznie z etykietami LBL
             
          UpdateButton("BTN_GRID_TOGGLE", "SET GRID", Global_GridPanel_Open);
      }

      // --- 3. Grid: Zmiana typu (BuyStop/SellStop) ---
      if(sparam == "BTN_GRID_TYPE") {
          Global_Grid_Direction = (Global_Grid_Direction == 0) ? 1 : 0;
          string dirText = (Global_Grid_Direction == 0) ? "MODE: BUY STOP" : "MODE: SELL STOP";
          color dirColor = (Global_Grid_Direction == 0) ? clrDodgerBlue : clrRed;
          ObjectSetString(0, "BTN_GRID_TYPE", OBJPROP_TEXT, dirText);
          ObjectSetInteger(0, "BTN_GRID_TYPE", OBJPROP_BGCOLOR, dirColor);
          ObjectSetInteger(0, "BTN_GRID_TYPE", OBJPROP_STATE, false);
      }

      // --- 4. Grid: Wykonanie (EXEC) ---
      if(sparam == "BTN_GRID_EXEC") {
          // Stan przycisku resetujemy wewnątrz ExecuteGrid() lub tutaj:
          ObjectSetInteger(0, "BTN_GRID_EXEC", OBJPROP_STATE, false);
          ExecuteGrid();
      }

      // --- 5. Funkcje Panic / Reset SL ---
      if(sparam == "BTN_REMOVE_SL") {
          ResetAllModes();
          RemoveAllStopLosses();
          UpdateButton("BTN_REMOVE_SL", "SL REMOVED", true);
      }

      if(sparam == "BTN_PANIC") {
          CloseAll(); 
          DeleteAllPending();
          ResetAllModes();
          ObjectSetInteger(0, "BTN_PANIC", OBJPROP_STATE, false);
      }

      // Krytyczne dla odświeżenia grafiki po usunięciu obiektów
      ChartRedraw();
   }
}

void ResetAllModes() {
    Global_CloseNetZero = false;
    Global_CloseEachZero = false;
    Global_MoneySL_Active = false;
    UpdateButton("BTN_NET_ZERO", "Pyramid Net Zero", false);
    UpdateButton("BTN_EACH_ZERO", "Each position Net Zero", false);
    UpdateButton("BTN_MONEY_SL", "Money SL Mode", false);
}

void CreateGUI() {
   CreateButton("BTN_NET_ZERO", 180, 30, "Pyramid Net Zero", Global_CloseNetZero);
   CreateButton("BTN_EACH_ZERO", 180, 70, "Each position Net Zero", Global_CloseEachZero);
   CreateButton("BTN_MONEY_SL", 180, 110, "Money SL Mode", Global_MoneySL_Active);
   CreateButton("BTN_GRID_TOGGLE", 180, 275, "SET GRID", false);
   
   if(ObjectFind(0, "EDT_MONEY_VAL") < 0) {
      ObjectCreate(0, "EDT_MONEY_VAL", OBJ_EDIT, 0, 0, 0);
      ObjectSetInteger(0, "EDT_MONEY_VAL", OBJPROP_CORNER, CORNER_RIGHT_UPPER);
      ObjectSetInteger(0, "EDT_MONEY_VAL", OBJPROP_XDISTANCE, 180);
      ObjectSetInteger(0, "EDT_MONEY_VAL", OBJPROP_YDISTANCE, 148); 
      ObjectSetInteger(0, "EDT_MONEY_VAL", OBJPROP_XSIZE, 170);
      ObjectSetInteger(0, "EDT_MONEY_VAL", OBJPROP_YSIZE, 25);
      ObjectSetString(0, "EDT_MONEY_VAL", OBJPROP_TEXT, "-10.00");
      ObjectSetInteger(0, "EDT_MONEY_VAL", OBJPROP_ALIGN, ALIGN_CENTER);
      ObjectSetInteger(0, "EDT_MONEY_VAL", OBJPROP_BGCOLOR, clrWhite);
   }

   CreateButton("BTN_REMOVE_SL", 180, 185, "REMOVE ALL SL", false);
   ObjectSetInteger(0, "BTN_REMOVE_SL", OBJPROP_BGCOLOR, clrDarkOrange);
   CreateButton("BTN_PANIC", 180, 230, "CLOSE ALL", false); 
   ObjectSetInteger(0, "BTN_PANIC", OBJPROP_BGCOLOR, clrRed);
}

void CreateButton(string name, int x, int y, string text, bool state) {
   ObjectCreate(0, name, OBJ_BUTTON, 0, 0, 0);
   ObjectSetInteger(0, name, OBJPROP_CORNER, CORNER_RIGHT_UPPER);
   ObjectSetInteger(0, name, OBJPROP_XDISTANCE, x);
   ObjectSetInteger(0, name, OBJPROP_YDISTANCE, y);
   ObjectSetInteger(0, name, OBJPROP_XSIZE, 170);
   ObjectSetInteger(0, name, OBJPROP_YSIZE, 35);
   ObjectSetString(0, name, OBJPROP_TEXT, text);
   ObjectSetInteger(0, name, OBJPROP_FONTSIZE, 9);
   UpdateButton(name, text, state);
}

void UpdateButton(string name, string text, bool state) {
   color btnColor;
   
   if(state) {
      // Kolory dla stanów AKTYWNYCH
      if(name == "BTN_PANIC")      btnColor = clrRed;
      else if(name == "BTN_REMOVE_SL") btnColor = clrDarkOrange;
      else                         btnColor = clrDodgerBlue;
   } else {
      // Kolor dla stanu WYŁĄCZONEGO
      btnColor = clrGray;
   }

   ObjectSetInteger(0, name, OBJPROP_BGCOLOR, btnColor);
   ObjectSetInteger(0, name, OBJPROP_COLOR, clrWhite);
   ObjectSetInteger(0, name, OBJPROP_STATE, state); 
}

void CloseAll() {
   for(int i = OrdersTotal() - 1; i >= 0; i--) {
      if(OrderSelect(i, SELECT_BY_POS, MODE_TRADES)) {
         if(OrderSymbol() == _Symbol && (MagicNumber == 0 || OrderMagicNumber() == MagicNumber)) {
            int type = OrderType();
            if(type == OP_BUY)  OrderClose(OrderTicket(), OrderLots(), Bid, 3, clrOrange);
            if(type == OP_SELL) OrderClose(OrderTicket(), OrderLots(), Ask, 3, clrOrange);
         }
      }
   }
}

void DeleteAllPending() {
   for(int i = OrdersTotal() - 1; i >= 0; i--) {
      if(OrderSelect(i, SELECT_BY_POS, MODE_TRADES)) {
         if(OrderSymbol() == _Symbol && (MagicNumber == 0 || OrderMagicNumber() == MagicNumber)) {
            if(OrderType() > 1) OrderDelete(OrderTicket());
         }
      }
   }
}

void CreateGridSubPanel() {
   // Zwiększamy xStart, aby panel nie nachodził na główne przyciski
   int xStart = 500; 
   int col2   = xStart - 90; 

   // --- LOGIKA AUTOMATYCZNEGO DOBORU WARTOŚCI ---
   string sym = _Symbol;
   string defaultStep = "25.0"; // Domyślnie dla Indeksów (DAX/Nasdaq)
   
   if(StringFind(sym, "BTC") >= 0) 
      defaultStep = "500.0";    // Dla Bitcoina krok 500$
   else if(StringFind(sym, "XAU") >= 0 || StringFind(sym, "GOLD") >= 0) 
      defaultStep = "5.0";      // Dla Złota krok 5$
   else if(Digits == 3 || Digits == 5) 
      defaultStep = "10.0";     // Dla Walut (Forex) 10 pipsów

   // 1. Wybór kierunku (BUY STOP / SELL STOP)
   CreateButton("BTN_GRID_TYPE", xStart, 30, (Global_Grid_Direction==0?"MODE: BUY STOP":"MODE: SELL STOP"), true);
   color dirColor = (Global_Grid_Direction == 0) ? clrDodgerBlue : clrRed;
   ObjectSetInteger(0, "BTN_GRID_TYPE", OBJPROP_BGCOLOR, dirColor);

   // 2. Pole: Lot Size
   ObjectCreate(0, "LBL_GRID_LOTS", OBJ_LABEL, 0, 0, 0);
   ObjectSetInteger(0, "LBL_GRID_LOTS", OBJPROP_CORNER, CORNER_RIGHT_UPPER);
   ObjectSetInteger(0, "LBL_GRID_LOTS", OBJPROP_XDISTANCE, xStart);
   ObjectSetInteger(0, "LBL_GRID_LOTS", OBJPROP_YDISTANCE, 65);
   ObjectSetString(0, "LBL_GRID_LOTS", OBJPROP_TEXT, "Lot Size:");

   ObjectCreate(0, "EDT_GRID_LOTS", OBJ_EDIT, 0, 0, 0);
   ObjectSetInteger(0, "EDT_GRID_LOTS", OBJPROP_CORNER, CORNER_RIGHT_UPPER);
   ObjectSetInteger(0, "EDT_GRID_LOTS", OBJPROP_XDISTANCE, xStart);
   ObjectSetInteger(0, "EDT_GRID_LOTS", OBJPROP_YDISTANCE, 80);
   ObjectSetInteger(0, "EDT_GRID_LOTS", OBJPROP_XSIZE, 80);
   ObjectSetInteger(0, "EDT_GRID_LOTS", OBJPROP_YSIZE, 25);
   ObjectSetString(0, "EDT_GRID_LOTS", OBJPROP_TEXT, "0.01"); 

   // 3. Pole: Step (Pips/Units)
   ObjectCreate(0, "LBL_GRID_STEP", OBJ_LABEL, 0, 0, 0);
   ObjectSetInteger(0, "LBL_GRID_STEP", OBJPROP_CORNER, CORNER_RIGHT_UPPER);
   ObjectSetInteger(0, "LBL_GRID_STEP", OBJPROP_XDISTANCE, xStart);
   ObjectSetInteger(0, "LBL_GRID_STEP", OBJPROP_YDISTANCE, 115);
   ObjectSetString(0, "LBL_GRID_STEP", OBJPROP_TEXT, "Step (Pips/Pts):");

   ObjectCreate(0, "EDT_GRID_STEP", OBJ_EDIT, 0, 0, 0);
   ObjectSetInteger(0, "EDT_GRID_STEP", OBJPROP_CORNER, CORNER_RIGHT_UPPER);
   ObjectSetInteger(0, "EDT_GRID_STEP", OBJPROP_XDISTANCE, xStart);
   ObjectSetInteger(0, "EDT_GRID_STEP", OBJPROP_YDISTANCE, 130);
   ObjectSetInteger(0, "EDT_GRID_STEP", OBJPROP_XSIZE, 80);
   ObjectSetInteger(0, "EDT_GRID_STEP", OBJPROP_YSIZE, 25);
   ObjectSetString(0, "EDT_GRID_STEP", OBJPROP_TEXT, defaultStep); // Wstawia zmienną zależną od rynku

   // 4. Pole: Count (Ilość zleceń)
   ObjectCreate(0, "LBL_GRID_COUNT", OBJ_LABEL, 0, 0, 0);
   ObjectSetInteger(0, "LBL_GRID_COUNT", OBJPROP_CORNER, CORNER_RIGHT_UPPER);
   ObjectSetInteger(0, "LBL_GRID_COUNT", OBJPROP_XDISTANCE, col2);
   ObjectSetInteger(0, "LBL_GRID_COUNT", OBJPROP_YDISTANCE, 115);
   ObjectSetString(0, "LBL_GRID_COUNT", OBJPROP_TEXT, "Count (Max 50):");

   ObjectCreate(0, "EDT_GRID_COUNT", OBJ_EDIT, 0, 0, 0);
   ObjectSetInteger(0, "EDT_GRID_COUNT", OBJPROP_CORNER, CORNER_RIGHT_UPPER);
   ObjectSetInteger(0, "EDT_GRID_COUNT", OBJPROP_XDISTANCE, col2);
   ObjectSetInteger(0, "EDT_GRID_COUNT", OBJPROP_YDISTANCE, 130);
   ObjectSetInteger(0, "EDT_GRID_COUNT", OBJPROP_XSIZE, 80);
   ObjectSetInteger(0, "EDT_GRID_COUNT", OBJPROP_YSIZE, 25);
   ObjectSetString(0, "EDT_GRID_COUNT", OBJPROP_TEXT, "3");

   // 5. Przycisk START
   CreateButton("BTN_GRID_EXEC", xStart, 170, "START GRID", false);
   ObjectSetInteger(0, "BTN_GRID_EXEC", OBJPROP_BGCOLOR, clrGreen);
   ObjectSetInteger(0, "BTN_GRID_EXEC", OBJPROP_XSIZE, 170); 
}

void RemoveGridSubPanel() {
   ObjectDelete(0, "BTN_GRID_TYPE");
   ObjectDelete(0, "LBL_GRID_STEP");
   ObjectDelete(0, "EDT_GRID_STEP");
   ObjectDelete(0, "EDT_GRID_COUNT");
   ObjectDelete(0, "BTN_GRID_EXEC");
   ObjectDelete(0, "EDT_GRID_LOTS"); // To dodałem
}


void ExecuteGrid() {
   // --- BLOKADA DUBELTA ---
   ObjectSetInteger(0, "BTN_GRID_EXEC", OBJPROP_STATE, false);
   
   // 1. Pobranie danych z pól edycyjnych
   double startLot  = StringToDouble(ObjectGetString(0, "EDT_GRID_LOTS", OBJPROP_TEXT));
   int    count     = (int)StringToInteger(ObjectGetString(0, "EDT_GRID_COUNT", OBJPROP_TEXT));
   double stepInput = StringToDouble(ObjectGetString(0, "EDT_GRID_STEP", OBJPROP_TEXT));
   
   // --- ZABEZPIECZENIA ---
   if(count > 50) { count = 50; ObjectSetString(0, "EDT_GRID_COUNT", OBJPROP_TEXT, "50"); }
   if(count <= 0 || stepInput <= 0 || startLot <= 0) {
      Print("Grid Error: Nieprawidłowe parametry wejściowe.");
      return;
   }

   // 2. Inteligentne przeliczanie kroku (Step)
   double stepSize;
   
   // Jeśli Forex (5/3 cyfry), przeliczamy pipsy na punkty (1 pips = 10 pkt)
   if(Digits == 3 || Digits == 5) {
      stepSize = stepInput * Point * 10; 
   } 
   // Jeśli Złoto, Indeksy, Krypto - traktujemy stepInput jako "czyste punkty ceny"
   // Przykład: DAX cena 15000.5, stepInput 10 -> następne zlecenie 15010.5
   else {
      stepSize = stepInput; 
   }

   RefreshRates();
   
   // Kierunek i cena startowa
   int type = (Global_Grid_Direction == 0) ? OP_BUYSTOP : OP_SELLSTOP;
   double basePrice = (type == OP_BUYSTOP) ? Ask : Bid;
   
   Print("Startowanie siatki: ", (type == OP_BUYSTOP ? "BUY STOP" : "SELL STOP"), " Krok: ", stepSize);

   for(int i = 0; i < count; i++) {
      double entryPrice;
      
      // Obliczanie ceny kolejnego zlecenia w siatce
      if(type == OP_BUYSTOP) 
          entryPrice = basePrice + (i + 1) * stepSize;
      else 
          entryPrice = basePrice - (i + 1) * stepSize;

      entryPrice = NormalizeDouble(entryPrice, Digits);

      // Wysłanie zlecenia
      int ticket = OrderSend(_Symbol, type, startLot, entryPrice, 3, 0, 0, "Grid ID:" + IntegerToString(i), MagicNumber, 0, (type == OP_BUYSTOP ? clrDodgerBlue : clrRed));
      
      if(ticket < 0) {
          int err = GetLastError();
          Print("BŁĄD ZLECENIA ", i, ": Kod błędu = ", err);
          if(err == 130) Print("Err 130: Cena ", entryPrice, " za blisko rynku (StopLevel/Spread).");
          if(err == 134) { Alert("BRAK ŚRODKÓW (Margin)!"); break; }
      }
   }
   
   ChartRedraw();
}



void HideGridSubPanel() {
   // Usuwamy przyciski i pola edycyjne
   ObjectDelete(0, "BTN_GRID_TYPE");
   ObjectDelete(0, "BTN_GRID_EXEC");
   ObjectDelete(0, "EDT_GRID_LOTS");
   ObjectDelete(0, "EDT_GRID_STEP");
   ObjectDelete(0, "EDT_GRID_COUNT");
   
   // Usuwamy etykiety tekstowe
   ObjectDelete(0, "LBL_GRID_LOTS");
   ObjectDelete(0, "LBL_GRID_STEP");
   ObjectDelete(0, "LBL_GRID_COUNT");
   
   ChartRedraw();
}

