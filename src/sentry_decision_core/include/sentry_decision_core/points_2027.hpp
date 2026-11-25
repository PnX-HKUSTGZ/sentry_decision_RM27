#pragma once

#include <array>
#include <string_view>

namespace sentry_decision {
namespace points2027 {

// 2027 命名点。名字与语义见 docs/POINTS_2027.md。
// 坐标由 config/maps/RMUC27.yaml 提供；Task 只引用这里的字符串，不写死坐标。
inline constexpr std::string_view kOwnBaseSupply = "own_base_supply";
inline constexpr std::string_view kOwnFort = "own_fort";
inline constexpr std::string_view kEnemyFort = "enemy_fort";
inline constexpr std::string_view kOwnOutpostBuff = "own_outpost_buff";
inline constexpr std::string_view kEnemyOutpostBuff = "enemy_outpost_buff";
inline constexpr std::string_view kOwnOutpostFront = "own_outpost_front";
inline constexpr std::string_view kEnemyOutpostFront = "enemy_outpost_front";
inline constexpr std::string_view kOwnOutpostSide = "own_outpost_side";
inline constexpr std::string_view kEnemyOutpostSide = "enemy_outpost_side";
inline constexpr std::string_view kOwnOutpostOpposite = "own_outpost_opposite";
inline constexpr std::string_view kEnemyOutpostOpposite = "enemy_outpost_opposite";
// 中央高地 / 敌方高地 / 己方高地范围点；半径来自地图配置，仅用于回血点判断。
inline constexpr std::string_view kHighlandAreaCenter = "highland_area_center";
inline constexpr std::string_view kHighlandAreaEnemy = "highland_area_enemy";
inline constexpr std::string_view kHighlandAreaOur = "highland_area_our";

inline constexpr std::array<std::string_view, 14> kAll = {
    kOwnBaseSupply,
    kOwnFort,
    kEnemyFort,
    kOwnOutpostBuff,
    kEnemyOutpostBuff,
    kOwnOutpostFront,
    kEnemyOutpostFront,
    kOwnOutpostSide,
    kEnemyOutpostSide,
    kOwnOutpostOpposite,
    kEnemyOutpostOpposite,
    kHighlandAreaCenter,
    kHighlandAreaEnemy,
    kHighlandAreaOur,
};

}  // namespace points2027
}  // namespace sentry_decision
