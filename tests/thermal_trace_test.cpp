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
  std::cout<<"Thermal trace pre/shot/post, rollover and bounds passed\n";
}
