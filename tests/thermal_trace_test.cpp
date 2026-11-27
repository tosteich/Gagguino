#include <cassert>
#include <cmath>
#include <iostream>
#include "../ThermalTraceCore.h"

int main() {
  ThermalTraceCore trace;
  // 25 seconds idle gives exactly the latest 20s of pre-roll.
  for(uint32_t t=0;t<25000;t+=200)
    trace.sample(false,t,93.0f,0.0f,10,0);
  assert(trace.state()==ThermalTraceCore::IDLE);
  trace.sample(true,25000,92.9f,1.2f,15,25);
  assert(trace.state()==ThermalTraceCore::FLOW);
  assert(trace.count()==101);
  assert(trace.at(0).relativeMs==-20000);
  assert(trace.at(99).relativeMs==-200);
  assert(trace.at(100).relativeMs==0);
  assert(trace.at(100).phase==ThermalTraceCore::BREW);

  for(uint32_t t=25200;t<35000;t+=200)
    trace.sample(true,t,91.0f,9.0f,30,70);
  trace.sample(false,35000,90.8f,0.0f,40,0);
  assert(trace.state()==ThermalTraceCore::POST);
  assert(trace.at(trace.count()-1).phase==ThermalTraceCore::AFTER);
  for(uint32_t t=35200;t<=95000;t+=200)
    trace.sample(false,t,93.0f,0.0f,0,0);
  assert(trace.state()==ThermalTraceCore::READY);
  assert(trace.count()<=ThermalTraceCore::MAX_SAMPLES);
  assert(trace.at(trace.count()-1).phase==ThermalTraceCore::AFTER);

  // A new flow begins a new trace and discards the old complete one.
  trace.sample(true,95200,93.0f,1.0f,10,30);
  assert(trace.state()==ThermalTraceCore::FLOW);
  assert(trace.count()==101);
  assert(trace.at(100).relativeMs==0);

  // Exhaust the long-shot buffer; pre/post sections must remain available.
  for(uint32_t t=95400;t<300000;t+=200)
    trace.sample(true,t,90.0f,9.0f,100,100);
  assert(trace.count()==ThermalTraceCore::PRE_SAMPLES+ThermalTraceCore::FLOW_SAMPLES);
  trace.sample(false,300000,89.0f,0.0f,0,0);
  for(uint32_t t=300200;t<=360000;t+=200)
    trace.sample(false,t,91.0f,0.0f,0,0);
  assert(trace.state()==ThermalTraceCore::READY);
  assert(trace.count()==ThermalTraceCore::MAX_SAMPLES);

  // millis rollover: all pre-roll timestamps remain negative relative to shot.
  ThermalTraceCore rollover;
  rollover.sample(false,UINT32_MAX-200,93,0,0,0);
  rollover.sample(false,UINT32_MAX,93,0,0,0);
  rollover.sample(true,199,92,1,10,10);
  assert(rollover.at(0).relativeMs==-400);
  assert(rollover.at(1).relativeMs==-200);
  assert(rollover.at(2).relativeMs==0);
  // Extended diagnostics must preserve precise sensor and both PID terms.
  ThermalTraceCore diagnostics;
  diagnostics.sample(false,0,93.214f,0.0f,12,0,1.2f,13.4f,-2.5f,
                     -3.5f,4.2f,-0.1f,17,93.0f);
  diagnostics.sample(true,200,93.217f,1.2f,14,50,1.3f,13.5f,-2.1f,
                     -3.4f,4.1f,-0.2f,18,93.0f);
  const auto &d=diagnostics.at(1);
  assert(d.preciseTemperature100==9322);
  assert(d.targetTemperature100==9300);
  assert(d.activeP10==13 && d.activeI10==135 && d.activeD10==-21);
  assert(d.shadowP10==-34 && d.shadowI10==41 && d.shadowD10==-2);
  assert(d.shadowPower==18);
  diagnostics.sample(false,400,93,0,0,0,0,0,0,0,0,0,-1,93);
  assert(diagnostics.at(2).shadowPower==255);

  std::cout<<"Thermal trace pre/shot/post, rollover and bounds passed\n";
}
