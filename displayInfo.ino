static float resolution = 255.0/110.0;

void updateGraphsInfo() {
  graphTemp = round(currentTemperature * resolution);
  graphPress = round(currentPressure * 10 * resolution);
}

//void moveBackGraphElement(byte index) {
//  updateGraphElement(index, GraphData[index + 1].Temp, GraphData[index + 1].Press);
//}
//
//void updateGraphElement(byte index, byte tempValue, byte pressValue) {
//  GraphData[index].Temp = tempValue;
//  GraphData[index].Press = pressValue;
//}

//void refreshPage2() {  
//  char tempData[16];  
//  for (byte i = 0; i < graphTick; i++) {
//    for (byte a = 0; a < 5; a++) {
//      sprintf(tempData, "add 9,0,%u", GraphData[i].Temp);   
//      myNex.writeStr(tempData); 
//      sprintf(tempData, "add 9,1,%u", GraphData[i].Press);  
//      myNex.writeStr(tempData); 
//    }
//  }  
//  myNex.writeNum("isLoaded.val", 1);
//}

//static byte lowTemp;
//static byte highTemp;
//static int z = graphLength - 1;
//static int tempGraph [graphLength];
//static byte middleTemp = 0;
//static boolean middleTempWasChanged = true;



//void drawTempGraph () {
//  if (abs(currentTemperature - middleTemp) > 3) {
//    middleTemp = currentTemperature;
//    middleTempWasChanged = true;
//  }
//  if (middleTempWasChanged) {
//    lowTemp = middleTemp - tempGraphOffset / resolution;
//    highTemp = middleTemp + tempGraphOffset / resolution;
//    tft.setCursor(0, (topGraphX - 3));
//    if (highTemp < 100) {
//      tft.print(" ");
//      tft.print(highTemp);
//    } else {
//      tft.print(highTemp);
//    }
//    tft.setCursor(0, (bottomGraphX - 3));
//    if (lowTemp < 100) {
//      tft.print(" ");
//      tft.print(lowTemp);
//    } else {
//      tft.print(lowTemp);
//    }
//    tft.setTextColor(TFT_WHITE, TFT_BLACK);
//    tft.setCursor(0, (middleGraphX - 3));
//    if (middleTemp < 100) {
//      tft.print(" ");
//      tft.print(middleTemp);
//    } else {
//      tft.print(middleTemp);
//    }
//    middleTempWasChanged = false;
//  }
//  int highLimit = highTemp * resolution;
//  int lowLimit = lowTemp * resolution;
//  int targetLimit = middleTemp * resolution;
//  for (int i = 0; i <= z; i++) {
//    if (i == z) {
//      tempGraph[i] = round(currentTemperature * resolution);
//    } else {
//      tempGraph[i] = tempGraph [i + 1];
//    }
//    int offsetX = graphStart + 1 + i;
//    tft.drawLine(offsetX, topGraphX, offsetX, bottomGraphX, TFT_BLACK);
//    if (isWithinLimits(tempGraph[i], highLimit, lowLimit)) {
//      if (isWithinLimits(tempGraph[i - 1], highLimit, lowLimit)) {
//        tft.drawLine((offsetX - 1), (middleGraphX - (tempGraph[i - 1] - targetLimit)), offsetX, (middleGraphX - (tempGraph[i] - targetLimit)), TFT_RED);
//      } else {
//        tft.drawPixel(offsetX, middleGraphX - (tempGraph[i] - targetLimit), TFT_RED);
//      }
//    }
//  }
//}
//
//boolean isWithinLimits (int temperature, int highLimit, int lowLimit) {
//  return temperature < highLimit && temperature > lowLimit;
//}
//
