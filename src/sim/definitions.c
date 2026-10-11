#include "game.h"

const char *RESOURCE_NAMES[] = {"Food", "Wood", "Stone", "Hides", "Tools", "Ore", "Bronze"};
const char *ROLE_NAMES[] = {"Builders", "Foragers", "Woodcutters", "Stone gatherers", "Hunters",
                            "Crafters", "Farmers",  "Guards",      "Miners"};
const char *JOB_NAMES[] = {"Finding work", "Gathering", "Building",   "Delivering", "Discovering",
                           "Crafting",     "Planting",  "Harvesting", "On watch",   "Defending",
                           "Moving",       "Repairing", "Smelting"};
const BuildingDef BUILDINGS[] = {
    {"Hearth", "Burns wood to keep the whole camp warm.", 2, 2, {0, 12, 8, 0, 0}, 400},
    {"Shelter", "Four beds. Upgrade after discovering insulation.", 2, 2, {0, 18, 2, 4, 0}, 650},
    {"Stockpile", "A delivery point for every resource.", 2, 2, {0, 12, 0, 0, 0}, 350},
    {"Foraging post", "Nearby foragers bring back extra food.", 2, 2, {0, 14, 0, 0, 0}, 450},
    {"Hunting camp", "Nearby hunters bring back extra meat and hides.", 2, 2, {0, 20, 4, 0, 0}, 550},
    {"Workshop", "Staff with crafters to discover and make tools.", 3, 2, {0, 30, 12, 0, 0}, 850},
    {"Granary", "A food delivery point. Keeps stored food fresh.", 2, 2, {0, 24, 8, 0, 0}, 600},
    {"Farm plot", "Fertile soil only. Plant, grow and harvest grain.", 2, 2, {0, 8, 0, 0, 1}, 250},
    {"Watch post", "Guards rally here and intercept nearby raiders.", 1, 1, {0, 18, 5, 0, 0}, 500},
    {"Palisade", "Blocks movement. Leave a gate for deliveries.", 1, 1, {0, 4, 0, 0, 0}, 160},
    {"Gate", "A passable entrance through your palisade.", 1, 1, {0, 8, 2, 0, 0}, 220},
    {"Training", "Train citizens into soldiers; equip bronze weapons.", 3, 2, {0, 35, 15, 4}, 900},
    {"Smelter", "Crafters turn 4 ore and 2 wood into 2 bronze.", 2, 2, {0, 40, 25, 0, 5}, 1000}};
const DiscoveryDef DISCOVERIES[] = {
    {"Better tools", "Workers bring back more supplies per trip.", {0, 16, 10, 0, 3}, 1800},
    {"Insulation", "Unlock warm huts: upgrade shelters for wood and hides.", {0, 12, 0, 6, 0}, 1800},
    {"Food storage", "Unlock granaries; reduce daily food spoilage.", {0, 18, 8, 0, 1}, 1800},
    {"Farming", "After the thaw, cultivate fertile river soil.", {10, 20, 10, 0, 2}, 2400},
    {"Metallurgy", "Unlock ore mining and smelting in the farming age.", {50, 50, 40, 0, 8}, 6000},
    {"Crop yield", "Double each harvest through better cultivation.", {60, 40, 20, 0, 5}, 3600},
    {"Stonework",
     "Bronze tools reinforce buildings. Repair to reach their new health limit.",
     {0, 100, 100, 0, 0, 0, 20},
     6000}};

const UnitDef UNITS[] = {{{0}, 0, 18, 12, 1, 12, 40, 0},
                         {{10, 10, 4, 2}, 300, 65, 45, 1, 12, 40, 12},
                         {{10, 5, 8, 2}, 300, 65, 45, 5, 18, 48, 0},
                         {{12, 12, 6, 2}, 300, 65, 180, 1, 12, 34, 0}};
const int RECRUIT_COST[RESOURCE_COUNT] = {12};
const int EQUIPMENT_COST[RESOURCE_COUNT] = {5, 0, 0, 0, 0, 0, 4};
const int RECRUIT_TICKS[AGE_COUNT] = {2400, 2400, 1800, 1050};
const RecipeDef TOOL_RECIPE = {{0, 2, 1}, TOOLS, 3, 300};
const RecipeDef BRONZE_RECIPE = {{0, 2, 0, 0, 0, 4}, BRONZE, 2, 600};

const char *UNIT_NAMES[] = {"Citizen", "Shield spearman", "Slinger", "Axeman"};
const AgeDef AGES[] = {{"Ice Age Tribe", {0}, 0, 8},
                       {"Settled Tribe", {40, 40, 20}, 1800, 8},
                       {"Farming Village", {120, 120, 60, 0, 10}, 2400, 16},
                       {"Bronze Age", {300, 200, 100, 0, 10, 0, 40}, 3600, 32}};
