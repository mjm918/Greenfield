#ifndef GREENFIELD_VIEW_ANIMATION_H
#define GREENFIELD_VIEW_ANIMATION_H
#include "context.h"
typedef struct
{
    const Atlas *atlas;
    int frame;
    bool flip;
} SpritePose;
SpritePose ViewCitizenPose(const ViewContext *ui, int id);
SpritePose ViewAnimalPose(const ViewContext *ui, int id);
Vector2 ViewAnimalPosition(const ViewContext *ui, int id);
Rectangle ViewPoseBounds(SpritePose pose, Vector2 foot, float cellHeight);
void ViewPoseDraw(SpritePose pose, Vector2 foot, float cellHeight, Color tint);
int ViewAnimalAt(const ViewContext *ui);
#endif
