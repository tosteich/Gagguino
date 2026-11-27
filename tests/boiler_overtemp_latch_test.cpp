#include <cassert>
#include <cmath>
#include <iostream>
#include "../BoilerOvertempLatch.h"

int main() {
  BoilerOvertempLatch normal;
  assert(!normal.observe(93.0f,93.0f));
  assert(!normal.observe(97.9f,93.0f));
  assert(normal.observe(98.0f,93.0f));
  assert(normal.observe(90.0f,93.0f)); // never reenable in this boot

  BoilerOvertempLatch steam;
  assert(!steam.observe(135.0f,135.0f));
  assert(steam.observe(140.0f,135.0f));

  BoilerOvertempLatch invalid;
  assert(invalid.observe(NAN,93.0f));

  BoilerOvertempLatch target;
  assert(target.observe(92.0f,0.0f));

  BoilerOvertempLatch selection;
  assert(selection.observe(97.0f,92.0f));
  // Normal 135 -> 93 C steam cooldown must NOT latch heater OFF forever.
  BoilerOvertempLatch cooldown;
  assert(!cooldown.observe(135.0f,135.0f));
  cooldown.beginSteamCooldown(135.0f,93.0f);
  assert(cooldown.coolingAfterSteam());
  for (float temp : {135.0f,125.0f,115.0f,103.0f,98.0f}) {
    assert(!cooldown.observe(temp,93.0f));
    assert(!cooldown.tripped());
    assert(cooldown.heatInhibited());
  }
  assert(!cooldown.observe(97.9f,93.0f));
  assert(!cooldown.coolingAfterSteam());
  assert(!cooldown.heatInhibited());
  assert(!cooldown.observe(88.0f,93.0f));
  assert(!cooldown.heatInhibited()); // heat may resume below target

  // Absolute threshold still trips, including while cooling after steam.
  BoilerOvertempLatch hotCooldown;
  hotCooldown.beginSteamCooldown(135.0f,93.0f);
  assert(hotCooldown.observe(140.0f,93.0f));
  assert(hotCooldown.heatInhibited());
  assert(hotCooldown.observe(88.0f,93.0f)); // latched

  // Entering steam cancels an unfinished cooldown but cannot clear a fault.
  BoilerOvertempLatch interrupted;
  interrupted.beginSteamCooldown(135.0f,93.0f);
  interrupted.cancelSteamCooldown();
  assert(!interrupted.heatInhibited());
  interrupted.observe(98.0f,93.0f);
  interrupted.beginSteamCooldown(135.0f,93.0f);
  assert(interrupted.tripped());

  std::cout << "Experimental boiler overtemperature latch passed\n";
}
