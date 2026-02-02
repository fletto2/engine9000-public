#include "goku.h"
#include "build/cutscene_demo.h"

typedef struct {
  uint32_t d_long;
  uint16_t d_short;
  uint16_t d_byte;
} test_struct_t;

volatile test_struct_t test_struct = {
  .d_long = 0xDEADF00D,
  .d_short = 0xAAAA,
  .d_byte = 0x555
};

static void
cutscene_updateCamera(void)
{

}

static void
cutscene_updateVblank(void)
{

}

static void
cutscene_update(void)
{
  if (shenron_cutscene_frame()) {
     goku.currentFrame = 0;
  }
}

scene_level_definition_t cutscene = {
    .init = shenron_cutscene_setup,
    .update = cutscene_update,
    .updateCamera = cutscene_updateCamera,
    .updateVblank = cutscene_updateVblank,
    .paletteSet = 1
};    
