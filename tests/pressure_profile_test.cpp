// g++ -std=c++17 -Wall -Wextra -Werror -I. tests/pressure_profile_test.cpp -o /tmp/pressure_profile_test && /tmp/pressure_profile_test
#include <cassert>
#include <climits>
#include <iostream>
#include "../PressureProfile.h"

int main() {
  const PressureProfile p{3,20,4,90,5,40};
  assert(pressureProfileSetpoint(p,0) == 20);
  assert(pressureProfileSetpoint(p,2999) == 20);
  assert(pressureProfileSetpoint(p,3000) == 90);
  assert(pressureProfileSetpoint(p,6999) == 90);
  assert(pressureProfileSetpoint(p,7000) == 80);
  assert(pressureProfileSetpoint(p,8000) == 70);
  assert(pressureProfileSetpoint(p,11000) == 40);
  assert(pressureProfileSetpoint(p,12000) == 40);
  assert(pressureProfileSetpoint(p,UINT32_MAX) == 40);
  const PressureProfile noDecline{0,0,0,85,0,30};
  assert(pressureProfileSetpoint(noDecline,0) == 85);
  assert(pressureProfileSetpoint(noDecline,UINT32_MAX) == 85);
  const PressureProfile noPreinfuse{0,50,2,95,1,0};
  assert(pressureProfileSetpoint(noPreinfuse,0) == 95);
  assert(pressureProfileSetpoint(noPreinfuse,2000) == 0);
  assert(pressureProfileSetpoint(noPreinfuse,3000) == 0);
  const PressureProfile rising{0,0,0,30,3,90};
  assert(pressureProfileSetpoint(rising,0) == 50);
  assert(pressureProfileSetpoint(rising,1000) == 70);
  assert(pressureProfileSetpoint(rising,2000) == 90);
  assert(pressureProfileSetpoint(rising,3000) == 90);
  const PressureProfile zero{0,0,0,0,0,0};
  assert(pressureProfileSetpoint(zero,0) == 0);
  std::cout << "Pressure profile transition tests passed\n";
}
