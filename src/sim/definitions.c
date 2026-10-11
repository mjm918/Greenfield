#include "game.h"

const char *RESOURCE_NAMES[] = {"Food", "Wood", "Stone", "Hides", "Tools"};
const char *ROLE_NAMES[] = {"Builders", "Foragers", "Woodcutters", "Stone gatherers",
                            "Hunters",  "Crafters", "Farmers",     "Guards"};
const char *JOB_NAMES[] = {"Finding work", "Gathering",  "Building", "Delivering", "Discovering", "Crafting",
                           "Planting",     "Harvesting", "On watch", "Defending",  "Resting"};
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
    {"Gate", "A passable entrance through your palisade.", 1, 1, {0, 8, 2, 0, 0}, 220}};
const DiscoveryDef DISCOVERIES[] = {
    {"Better tools", "Workers bring back more supplies per trip.", {0, 16, 10, 0, 3}, 1800},
    {"Insulation", "Unlock warm huts: upgrade shelters for wood and hides.", {0, 12, 0, 6, 0}, 1800},
    {"Food storage", "Unlock granaries; reduce daily food spoilage.", {0, 18, 8, 0, 1}, 1800},
    {"Farming", "After the thaw, cultivate fertile river soil.", {10, 20, 10, 0, 2}, 2400}};
