#ifndef BASIC_HEADER_FILES_H
#define BASIC_HEADER_FILES_H

enum class team_reward {DEFAULT, ENABLED, DISABLED};
enum class auto_claim {DEFAULT, DISABLED, ENABLED, DISABLED_TOAST, ENABLED_TOAST};

struct vec2
{
    int x;
    int y;
};

enum class Shapes {DEFAULT, CIRCLE, SQUARE, ROUNDED_SQUARE, DIAMOND, PENTAGON, HEXAGON, OCTAGON, HEART, GEAR, NO_SHAPE}; //shape
enum class Progression {DEFAULT, LINEAR, FLEXIBLE};
enum class DependecyMode {ALL_COMPLETED, ONE_COMPLETED, ALL_STARTED, ONE_STARTED};
enum class Hide {DEFAULT, HIDDEN, VISIBLE};
#endif