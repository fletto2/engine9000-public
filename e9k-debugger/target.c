#include "debugger.h"
#include "target.h"

target_iface_t* target_targets[3];
target_iface_t* target;
static size_t target_targetCount = 0;

void
target_ctor(void)
{
    target_targets[TARGET_AMIGA] = target_amiga();
    target_targets[TARGET_NEOGEO] = target_neogeo();
    target_targetCount = 2;
#if E9K_ENABLE_MEGADRIVE
    target_targets[TARGET_MEGADRIVE] = target_megadrive();
    target_targetCount = 3;
#endif
}

void
target_settingsClearAllOptions(void)
{
    for (size_t i = 0; i < target_targetCount; i++) {
        if (target_targets[i]->settingsClearOptions) {
            target_targets[i]->settingsClearOptions();
        }
    }
}

int
target_isDefaultCorePath(const char *path)
{
    if (!path || !*path) {
        return 0;
    }
    for (size_t i = 0; i < target_targetCount; i++) {
        const char *defaultPath = target_targets[i]->defaultCorePath ? target_targets[i]->defaultCorePath() : NULL;
        if (defaultPath && *defaultPath && strcmp(defaultPath, path) == 0) {
            return 1;
        }
    }
    return 0;
}




void
target_setTarget(target_iface_t* newTarget)
{
  target = newTarget;
  debugger.config.target = newTarget;
}

void
target_setTargetIndex(int index)
{
  if (index >= 0 && index < (int)target_targetCount) {
    target = target_targets[index];
    debugger.config.target = target;
    return;
  }

  debug_printf("target_setTargetIndex: invalid target index %d\n", index);
}

void
target_nextTarget(void)
{
  size_t i;
  for (i = 0; i < target_targetCount; i++) {
    if (target == target_targets[i]) {
      break;
    }
  }

  i++;
  i = i % target_targetCount;
  target = target_targets[i];
  debugger.config.target = target;
}
