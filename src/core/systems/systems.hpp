#pragma once
#include "schemas/Schemas.h"
#include "entities/Entities.h"
#include "local/Local.h"
#include "prediction/Prediction.h"
#include "view/View.h"
#include "bones/Bones.h"
#include "hitboxes/Hitboxes.h"

namespace systems {

    inline entities   g_entities{};
    inline local      g_local{};
    inline prediction g_prediction{};
    inline view       g_view{};
    inline bones      g_bones{};
    inline hitboxes   g_hitboxes{};

}