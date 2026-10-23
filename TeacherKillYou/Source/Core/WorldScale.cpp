#include "WorldScale.h"

namespace WorldScale {
    // 現行校舎の床から天井まで約4.65を基準に、体格と目線を合わせる。
    // これらはメートル換算値ではなく、このマップでのワールド単位。
    const float StageScale = 1.25f;
    const CharacterSize Player{2.9f, .36f};
    const CharacterSize Monster{3.1f, .42f};
    const CharacterSize Zombie{2.9f, .36f};
    const float EyeHeightRatio = .91f;
}
