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
double Global_InitialProfit = 0; // Tu bot zapamięta zysk z chwili kliknięcia
double Global_InitialProfit_NetZero = 0; // Dla Pyramid Net Zero (TEGO BRAKOWAŁO)
// --- Pamiec ustawien Grid
string Last_Grid_Lot = "0.01";
string Last_Grid_Step = ""; // Puste, aby przy pierwszym uruchomieniu zadzialala logika symbolu
string Last_Grid_Count = "5";


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
   double totalSLMoney = 0; // Nowa zmienna na wartość SL w USD
   int activeMarketPositions = 0;
   bool pendingToClear = false; 
   double pipsToPoints = (Digits == 3 || Digits == 5) ? Point * 10 : Point;

   // Parametry do obliczeń pieniężnych SL
   double tickVal = MarketInfo(_Symbol, MODE_TICKVALUE);
   double tickSz  = MarketInfo(_Symbol, MODE_TICKSIZE);

   // --- 1. SKANOWANIE POZYCJI ---
   for(int i = OrdersTotal() - 1; i >= 0; i--) {
      if(OrderSelect(i, SELECT_BY_POS, MODE_TRADES)) {
         if(OrderSymbol() == _Symbol && (MagicNumber == 0 || OrderMagicNumber() == MagicNumber)) {
            int type = OrderType();
            if(type == OP_BUY || type == OP_SELL) {
               // Obliczanie zysku netto
               double profit = OrderProfit() + OrderSwap() + OrderCommission();
               totalNetProfit += profit;
               activeMarketPositions++;

               // Obliczanie wartości SL w USD (jeśli ustawiony)
               if(OrderStopLoss() > 0 && tickVal > 0 && tickSz > 0) {
                  double slDist = (type == OP_BUY) ? (OrderStopLoss() - OrderOpenPrice()) : (OrderOpenPrice() - OrderStopLoss());
                  totalSLMoney += (slDist / tickSz) * tickVal * OrderLots();
               }

               if(Global_CloseEachZero) {
                  double openPrice = OrderOpenPrice();
                  double currentPrice = (type == OP_BUY) ? Bid : Ask;
                  double pipsProfit = (type == OP_BUY) ? (currentPrice - openPrice) : (openPrice - currentPrice);
                  pipsProfit /= pipsToPoints;

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

// --- WYŚWIETLANIE ZINTEGROWANE (WERSJA PANCERNA - ANCHOR RIGHT) ---
   double balance = AccountBalance();
   double profitPct = (balance > 0) ? (totalNetProfit / balance) * 100.0 : 0;
   double slPct     = (balance > 0) ? (totalSLMoney / balance) * 100.0 : 0;

   // Budujemy jeden ciąg
   string fullText = "Total SL: " + DoubleToString(totalSLMoney, 2) + " USD (" + DoubleToString(slPct, 2) + "%)" +
                     "  |  " + 
                     "Net: " + DoubleToString(totalNetProfit, 2) + " USD (" + DoubleToString(profitPct, 2) + "%)";

   if(ObjectFind(0, "LBL_DASHBOARD") < 0) {
       ObjectCreate(0, "LBL_DASHBOARD", OBJ_LABEL, 0, 0, 0);
       ObjectSetInteger(0, "LBL_DASHBOARD", OBJPROP_CORNER, CORNER_RIGHT_UPPER);
       // KLUCZOWA POPRAWKA: Kotwiczymy do prawej, żeby tekst rósł w LEWO
       ObjectSetInteger(0, "LBL_DASHBOARD", OBJPROP_ANCHOR, ANCHOR_RIGHT); 
       ObjectSetInteger(0, "LBL_DASHBOARD", OBJPROP_YDISTANCE, 10);
   }

   // Teraz X=180 to odległość PRAWEGO końca napisu od krawędzi
   ObjectSetInteger(0, "LBL_DASHBOARD", OBJPROP_XDISTANCE, 180);
   ObjectSetInteger(0, "LBL_DASHBOARD", OBJPROP_FONTSIZE, 10);
   ObjectSetString(0, "LBL_DASHBOARD", OBJPROP_TEXT, fullText);

   color dashboardColor = (totalNetProfit >= 0) ? clrLime : clrRed;
   ObjectSetInteger(0, "LBL_DASHBOARD", OBJPROP_COLOR, dashboardColor);

   // Czyścimy stare śmieci, jeśli zostały
   ObjectDelete(0, "LBL_LIVE_PROFIT");
   ObjectDelete(0, "LBL_LIVE_SL_TOTAL");

   // --- 2. LOGIKA MONEY STOP LOSS ---
   static bool BasketStarted = false; 
   if(Global_MoneySL_Active) {
      if(activeMarketPositions > 0) BasketStarted = true;
      if(activeMarketPositions == 0 && BasketStarted) {
        DeleteAllPending(); ResetAllModes(); BasketStarted = false; return;
      }
   
      if(activeMarketPositions > 0) {
         static uint lastMilliTime = 0;
         if(GetTickCount() - lastMilliTime > 2000) { 
             RefreshGlobalSL(); 
             lastMilliTime = GetTickCount(); 
         }

         Global_MoneySL_Value = StringToDouble(ObjectGetString(0, "EDT_MONEY_VAL", OBJPROP_TEXT));
         bool triggerSL = false;

         if(Global_MoneySL_Value < 0) {
            if(totalNetProfit <= Global_MoneySL_Value) triggerSL = true;
         } 
         else if(Global_MoneySL_Value > 0) {
            if(Global_InitialProfit < Global_MoneySL_Value) {
               if(totalNetProfit >= Global_MoneySL_Value) triggerSL = true;
            } 
            else {
               if(totalNetProfit <= Global_MoneySL_Value) triggerSL = true;
            }
         }

         if(triggerSL) { 
             Print("Money SL Mode: Zamknięto przy zysku: ", totalNetProfit, " (Cel: ", Global_MoneySL_Value, ")");
             CloseAll(); DeleteAllPending(); ResetAllModes(); BasketStarted = false; return; 
         }
      }
   }

   // --- 3. PIRAMIDA NET ZERO ---
   if(Global_CloseNetZero && activeMarketPositions >= 2 && totalNetProfit <= 0) {
      Print("Pyramid Net Zero: Hard Kill przy zysku <= 0.");
      CloseAll(); 
      DeleteAllPending();
      ResetAllModes();
      BasketStarted = false;
      return;
   }

   if(pendingToClear) DeleteAllPending();
}

//+------------------------------------------------------------------+
//| Funkcje Zarządzania SL                                           |
//+------------------------------------------------------------------+
void RefreshGlobalSL() {
   if(!Global_MoneySL_Active) return;

   // 1. Pobieranie parametrów rynkowych i GUI
   double targetMoney = StringToDouble(ObjectGetString(0, "EDT_MONEY_VAL", OBJPROP_TEXT));
   double totalLots = 0, weightedOpenPrice = 0;
   int type = -1, count = 0;

   // 2. Skanowanie koszyka pozycji
   for(int i = OrdersTotal() - 1; i >= 0; i--) {
      if(OrderSelect(i, SELECT_BY_POS, MODE_TRADES)) {
         if(OrderSymbol() == _Symbol && (MagicNumber == 0 || OrderMagicNumber() == MagicNumber)) {
            if(OrderType() <= 1) { // Tylko OP_BUY (0) i OP_SELL (1)
               totalLots += OrderLots();
               weightedOpenPrice += OrderOpenPrice() * OrderLots();
               type = OrderType();
               count++;
            }
         }
      }
   }

   if(count == 0 || totalLots <= 0) return;
   
   double avgOpenPrice = weightedOpenPrice / totalLots;
   
   // --- MATEMATYKA UNIWERSALNA (Zastępuje contractSize) ---
   double tickVal = MarketInfo(_Symbol, MODE_TICKVALUE);
   double tickSz  = MarketInfo(_Symbol, MODE_TICKSIZE);
   
   if(tickVal <= 0 || tickSz <= 0) return;

   // Obliczanie dystansu ceny w oparciu o TickValue (najdokładniejsza metoda)
   // Dystans = (Kasa / (Loty * Wartość_Punktu)) * Rozmiar_Punktu
   double priceDistance = (targetMoney / (totalLots * tickVal)) * tickSz;
   
   double slPrice = (type == OP_BUY) ? avgOpenPrice + priceDistance : avgOpenPrice - priceDistance;
   slPrice = NormalizeDouble(slPrice, Digits);

   // 3. Weryfikacja StopLevel (RefreshRates() jest krytyczne przed sprawdzeniem Bid/Ask)
   RefreshRates();
   double stopLevel = MarketInfo(_Symbol, MODE_STOPLEVEL) * Point;
   double currentPrice = (type == OP_BUY) ? Bid : Ask;
   
   bool distanceOk = (type == OP_BUY) ? (currentPrice - slPrice > stopLevel) : (slPrice - currentPrice > stopLevel);
   if(!distanceOk) return; 

   // 4. Wykonanie modyfikacji z bramką anty-spam (10 * Point)
   for(int i = OrdersTotal() - 1; i >= 0; i--) {
      if(OrderSelect(i, SELECT_BY_POS, MODE_TRADES)) {
         if(OrderSymbol() == _Symbol && (MagicNumber == 0 || OrderMagicNumber() == MagicNumber)) {
            
            double currentSL = OrderStopLoss();
            // Modyfikacja następuje tylko, gdy różnica jest istotna (redukcja logów)
            if(MathAbs(currentSL - slPrice) > 10 * Point) {
               if(!OrderModify(OrderTicket(), OrderOpenPrice(), slPrice, OrderTakeProfit(), 0, clrOrange)) {
                  // Opcjonalne: Print("Błąd modyfikacji: ", GetLastError());
               }
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
// --- Logika Net Zero ---
      if(sparam == "BTN_NET_ZERO") {
          Global_CloseNetZero = !Global_CloseNetZero;
          
          if(Global_CloseNetZero) { // Pobieraj tylko przy AKTYWACJI
              RefreshRates(); 
              double currentProfit = 0;
              for(int i = OrdersTotal() - 1; i >= 0; i--) {
                  if(OrderSelect(i, SELECT_BY_POS) && OrderSymbol() == _Symbol && (MagicNumber == 0 || OrderMagicNumber() == MagicNumber)) {
                      if(OrderType() <= 1) currentProfit += (OrderProfit() + OrderSwap() + OrderCommission());
                  }
              }
              Global_InitialProfit_NetZero = currentProfit;
              Print("Net Zero Aktywne. Start Profit: ", Global_InitialProfit_NetZero);
          }
          
          UpdateButton("BTN_NET_ZERO", "Pyramid Net Zero", Global_CloseNetZero);
      }

// --- Logika Money SL ---
      if(sparam == "BTN_MONEY_SL") {
          Global_MoneySL_Active = !Global_MoneySL_Active;
          
          if(Global_MoneySL_Active) { // Pobieraj tylko przy AKTYWACJI
              RefreshRates();
              double currentProfit = 0;
              for(int i = OrdersTotal() - 1; i >= 0; i--) {
                  if(OrderSelect(i, SELECT_BY_POS) && OrderSymbol() == _Symbol && (MagicNumber == 0 || OrderMagicNumber() == MagicNumber)) {
                      if(OrderType() <= 1) currentProfit += (OrderProfit() + OrderSwap() + OrderCommission());
                  }
              }
              Global_InitialProfit = currentProfit;
              Print("Money SL Aktywne. Start Profit: ", Global_InitialProfit);
          }
          
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
      
      // --- TUTAJ WKLEJ NOWY PRZYCISK ---
      if(sparam == "BTN_DEL_PENDING") {
          ManualDeletePending(); // Wywolujemy Twoja nowa funkcje
          
          // Natychmiast "wyciskamy" przycisk z powrotem, zeby nie swiecil na niebiesko
          ObjectSetInteger(0, "BTN_DEL_PENDING", OBJPROP_STATE, false);
          ChartRedraw();
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
    //UpdateButton("BTN_EACH_ZERO", "Each position Net Zero", false);
    UpdateButton("BTN_MONEY_SL", "Money SL Mode", false);
}
void CreateGUI() {
   CreateButton("BTN_NET_ZERO", 180, 30, "Pyramid Net Zero", Global_CloseNetZero);
   CreateButton("BTN_MONEY_SL", 180, 60, "Money SL Mode", Global_MoneySL_Active);
   
   if(ObjectFind(0, "EDT_MONEY_VAL") < 0) {
      ObjectCreate(0, "EDT_MONEY_VAL", OBJ_EDIT, 0, 0, 0);
      ObjectSetInteger(0, "EDT_MONEY_VAL", OBJPROP_CORNER, CORNER_RIGHT_UPPER);
      ObjectSetInteger(0, "EDT_MONEY_VAL", OBJPROP_XDISTANCE, 180);
      ObjectSetInteger(0, "EDT_MONEY_VAL", OBJPROP_YDISTANCE, 90); 
      ObjectSetInteger(0, "EDT_MONEY_VAL", OBJPROP_XSIZE, 140);
      ObjectSetInteger(0, "EDT_MONEY_VAL", OBJPROP_YSIZE, 20);
      ObjectSetString(0, "EDT_MONEY_VAL", OBJPROP_TEXT, "-10.00");
      ObjectSetInteger(0, "EDT_MONEY_VAL", OBJPROP_ALIGN, ALIGN_CENTER);
      ObjectSetInteger(0, "EDT_MONEY_VAL", OBJPROP_BGCOLOR, clrWhite);
   }

   CreateButton("BTN_REMOVE_SL", 180, 125, "REMOVE ALL SL", false);
   ObjectSetInteger(0, "BTN_REMOVE_SL", OBJPROP_BGCOLOR, clrDarkOrange);
   
   CreateButton("BTN_PANIC", 180, 160, "CLOSE ALL", false); 
   ObjectSetInteger(0, "BTN_PANIC", OBJPROP_BGCOLOR, clrRed);

   // --- POPRAWKA POZYCJI ---
   CreateButton("BTN_DEL_PENDING", 180, 195, "DELETE PENDING", false); // Y = 195
   ObjectSetInteger(0, "BTN_DEL_PENDING", OBJPROP_BGCOLOR, clrMediumSlateBlue);

   CreateButton("BTN_GRID_TOGGLE", 180, 230, "SET GRID", false); // Y = 230 (Zjechał niżej o 35)
}

void CreateButton(string name, int x, int y, string text, bool state) {
   ObjectCreate(0, name, OBJ_BUTTON, 0, 0, 0);
   ObjectSetInteger(0, name, OBJPROP_CORNER, CORNER_RIGHT_UPPER);
   ObjectSetInteger(0, name, OBJPROP_XDISTANCE, x);
   ObjectSetInteger(0, name, OBJPROP_YDISTANCE, y);
   ObjectSetInteger(0, name, OBJPROP_XSIZE, 140);
   ObjectSetInteger(0, name, OBJPROP_YSIZE, 25);
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
   // Ustawiamy xStart na 180, żeby było w tej samej linii co główne przyciski
   int xStart = 180; 
   int col2_x = xStart - 75; // Druga kolumna dla Count (obok Step)

   // --- LOGIKA AUTOMATYCZNEGO DOBORU WARTOŚCI ---
   string sym = _Symbol;
   string defaultStep = "25.0";
   
   if(StringFind(sym, "BTC") >= 0) defaultStep = "100.0"; 
   else if(StringFind(sym, "XAU") >= 0 || StringFind(sym, "GOLD") >= 0) defaultStep = "10.0"; 
   else if(StringFind(sym, "XAG") >= 0 || StringFind(sym, "SILVER") >= 0) defaultStep = "50.0"; 
   else if(Digits == 3 || Digits == 5) defaultStep = "10.0";

   // 1. Wybór kierunku - Y startuje od 315 (pod SET GRID)
   CreateButton("BTN_GRID_TYPE", xStart, 265, (Global_Grid_Direction==0?"MODE: BUY STOP":"MODE: SELL STOP"), true);
   color dirColor = (Global_Grid_Direction == 0) ? clrDodgerBlue : clrRed;
   ObjectSetInteger(0, "BTN_GRID_TYPE", OBJPROP_BGCOLOR, dirColor);

   // 2. Pole: Lot Size
   ObjectCreate(0, "LBL_GRID_LOTS", OBJ_LABEL, 0, 0, 0);
   ObjectSetInteger(0, "LBL_GRID_LOTS", OBJPROP_CORNER, CORNER_RIGHT_UPPER);
   ObjectSetInteger(0, "LBL_GRID_LOTS", OBJPROP_XDISTANCE, xStart);
   ObjectSetInteger(0, "LBL_GRID_LOTS", OBJPROP_YDISTANCE, 300);
   ObjectSetString(0, "LBL_GRID_LOTS", OBJPROP_TEXT, "Lot Size:");

   ObjectCreate(0, "EDT_GRID_LOTS", OBJ_EDIT, 0, 0, 0);
   ObjectSetInteger(0, "EDT_GRID_LOTS", OBJPROP_CORNER, CORNER_RIGHT_UPPER);
   ObjectSetInteger(0, "EDT_GRID_LOTS", OBJPROP_XDISTANCE, xStart);
   ObjectSetInteger(0, "EDT_GRID_LOTS", OBJPROP_YDISTANCE, 320);
   ObjectSetInteger(0, "EDT_GRID_LOTS", OBJPROP_XSIZE, 140); // Pełna szerokość panelu
   ObjectSetInteger(0, "EDT_GRID_LOTS", OBJPROP_YSIZE, 25);
   ObjectSetString(0, "EDT_GRID_LOTS", OBJPROP_TEXT, Last_Grid_Lot); 

   // 3. Pole: Step
   ObjectCreate(0, "LBL_GRID_STEP", OBJ_LABEL, 0, 0, 0);
   ObjectSetInteger(0, "LBL_GRID_STEP", OBJPROP_CORNER, CORNER_RIGHT_UPPER);
   ObjectSetInteger(0, "LBL_GRID_STEP", OBJPROP_XDISTANCE, xStart);
   ObjectSetInteger(0, "LBL_GRID_STEP", OBJPROP_YDISTANCE, 355);
   ObjectSetString(0, "LBL_GRID_STEP", OBJPROP_TEXT, "Step:");

   ObjectCreate(0, "EDT_GRID_STEP", OBJ_EDIT, 0, 0, 0);
   ObjectSetInteger(0, "EDT_GRID_STEP", OBJPROP_CORNER, CORNER_RIGHT_UPPER);
   ObjectSetInteger(0, "EDT_GRID_STEP", OBJPROP_XDISTANCE, xStart);
   ObjectSetInteger(0, "EDT_GRID_STEP", OBJPROP_YDISTANCE, 370);
   ObjectSetInteger(0, "EDT_GRID_STEP", OBJPROP_XSIZE, 65);
   ObjectSetInteger(0, "EDT_GRID_STEP", OBJPROP_YSIZE, 25);

   string stepToShow = (Last_Grid_Step == "") ? defaultStep : Last_Grid_Step; // Sprawdz czy mamy cos w pamieci
   ObjectSetString(0, "EDT_GRID_STEP", OBJPROP_TEXT, stepToShow);

   // 4. Pole: Count (Ustawione domyślnie na 5)
   ObjectCreate(0, "LBL_GRID_COUNT", OBJ_LABEL, 0, 0, 0);
   ObjectSetInteger(0, "LBL_GRID_COUNT", OBJPROP_CORNER, CORNER_RIGHT_UPPER);
   ObjectSetInteger(0, "LBL_GRID_COUNT", OBJPROP_XDISTANCE, col2_x);
   ObjectSetInteger(0, "LBL_GRID_COUNT", OBJPROP_YDISTANCE, 355);
   ObjectSetString(0, "LBL_GRID_COUNT", OBJPROP_TEXT, "Count:");

   ObjectCreate(0, "EDT_GRID_COUNT", OBJ_EDIT, 0, 0, 0);
   ObjectSetInteger(0, "EDT_GRID_COUNT", OBJPROP_CORNER, CORNER_RIGHT_UPPER);
   ObjectSetInteger(0, "EDT_GRID_COUNT", OBJPROP_XDISTANCE, col2_x);
   ObjectSetInteger(0, "EDT_GRID_COUNT", OBJPROP_YDISTANCE, 370);
   ObjectSetInteger(0, "EDT_GRID_COUNT", OBJPROP_XSIZE, 65);
   ObjectSetInteger(0, "EDT_GRID_COUNT", OBJPROP_YSIZE, 25);
   ObjectSetString(0, "EDT_GRID_COUNT", OBJPROP_TEXT, Last_Grid_Count);

   // 5. Przycisk START (Nazwa BTN_GRID_EXEC - musi się zgadzać z OnChartEvent)
   CreateButton("BTN_GRID_EXEC", xStart, 405, "START GRID", false);
   ObjectSetInteger(0, "BTN_GRID_EXEC", OBJPROP_BGCOLOR, clrGreen);
   ObjectSetInteger(0, "BTN_GRID_EXEC", OBJPROP_XSIZE, 140); 
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
   // 1. Zapisz wszystko jednym ciągiem
   if(ObjectFind(0, "EDT_GRID_LOTS") >= 0)  Last_Grid_Lot = ObjectGetString(0, "EDT_GRID_LOTS", OBJPROP_TEXT);
   if(ObjectFind(0, "EDT_GRID_STEP") >= 0)  Last_Grid_Step = ObjectGetString(0, "EDT_GRID_STEP", OBJPROP_TEXT);
   if(ObjectFind(0, "EDT_GRID_COUNT") >= 0) Last_Grid_Count = ObjectGetString(0, "EDT_GRID_COUNT", OBJPROP_TEXT);
   
   // 2. Usuwamy obiekty
   ObjectDelete(0, "BTN_GRID_TYPE");
   ObjectDelete(0, "BTN_GRID_EXEC");
   ObjectDelete(0, "EDT_GRID_LOTS");
   ObjectDelete(0, "EDT_GRID_STEP");
   ObjectDelete(0, "EDT_GRID_COUNT");
   ObjectDelete(0, "LBL_GRID_LOTS");
   ObjectDelete(0, "LBL_GRID_STEP");
   ObjectDelete(0, "LBL_GRID_COUNT");
   
   ChartRedraw();
}



void ManualDeletePending() {
   for(int i = OrdersTotal() - 1; i >= 0; i--) {
      if(OrderSelect(i, SELECT_BY_POS, MODE_TRADES)) {
         // Sprawdzamy symbol i Magic Number
         if(OrderSymbol() == _Symbol && (MagicNumber == 0 || OrderMagicNumber() == MagicNumber)) {
            // Interesują nas tylko typy: 2 (Buy Limit), 3 (Sell Limit), 4 (Buy Stop), 5 (Sell Stop)
            if(OrderType() > 1) { 
               if(!OrderDelete(OrderTicket())) {
                  Print("Failed to delete pending order #", OrderTicket(), " Error: ", GetLastError());
               }
            }
         }
      }
   }
   Print("Manual deletion of pending orders completed.");
   ChartRedraw();
}