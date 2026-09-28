#include "CheeseTrail.h"

#include "raymath.h"

void CheeseTrail::Reset(Vector2 head)
{
    points_.clear();
    points_.push_front(head);
}

void CheeseTrail::Record(Vector2 head)
{
    if (points_.empty() || Vector2Distance(points_.front(), head) >= MinStep) points_.push_front(head);
    if (points_.size() > MaxPoints) points_.pop_back();
}

Vector2 CheeseTrail::PointBehind(Vector2 head, float distance) const
{
    Vector2 prev = head;
    float remaining = distance;
    for (const Vector2& p : points_)
    {
        const float seg = Vector2Distance(prev, p);
        if (seg >= remaining && seg > 0.0f) return Vector2Lerp(prev, p, remaining / seg);
        remaining -= seg;
        prev = p;
    }
    return prev; // trail not long enough yet: bunch up at the oldest point
}
