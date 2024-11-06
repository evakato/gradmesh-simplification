#include "gms_math.hpp"

AABB bezierAABB(glm::vec2 p0, glm::vec2 p1, glm::vec2 p2, glm::vec2 p3)
{
    AABB aabb{glm::min(p0, p3), glm::max(p0, p3)};
    auto [_, c, b, a] = applyCubicBasis({p0, p1, p2, p3}, bezierBasisMat);
    // auto c = (p1 - p0);
    // auto b = (p0 - (2.0f * p1) + p2);
    // auto a = (-p0 + (3.0f * p1) - (3.0f * p2) + p3);
    a *= 3.0f;
    auto discrim = (b * b - a * c);
    auto tNeg = (-b - glm::sqrt(discrim)) / (a);
    auto tPos = (-b + glm::sqrt(discrim)) / (a);
    if (tNeg.x > 0.0f && tNeg.x < 1.0f)
        aabb.expand(interpolateCubic(p0, p1, p2, p3, tNeg.x, bezierBasisMat));
    if (tPos.x > 0.0f && tPos.x < 1.0f)
        aabb.expand(interpolateCubic(p0, p1, p2, p3, tPos.x, bezierBasisMat));
    if (tNeg.y > 0.0f && tNeg.y < 1.0f)
        aabb.expand(interpolateCubic(p0, p1, p2, p3, tNeg.y, bezierBasisMat));
    if (tPos.y > 0.0f && tPos.y < 1.0f)
        aabb.expand(interpolateCubic(p0, p1, p2, p3, tPos.y, bezierBasisMat));
    return aabb;
}

AABB hermiteAABB(glm::vec2 p0, glm::vec2 p1, glm::vec2 p2, glm::vec2 p3)
{
    AABB aabb{glm::min(p0, p3), glm::max(p0, p3)};
    auto [_, c, b, a] = applyCubicBasis({p0, p1, p2, p3}, hermiteBasisMat);
    a *= 3.0f;
    auto discrim = b * b - a * c;

    for (int i = 0; i < 2; ++i)
    {
        if (discrim[i] < 0.0f)
            continue;
        float sqrtDiscrim = glm::sqrt(discrim[i]);
        float tm = (-b[i] - sqrtDiscrim) / a[i];
        float tp = (-b[i] + sqrtDiscrim) / a[i];
        if (tm > 0.0f && tm < 1.0f)
            aabb.expand(interpolateCubic(p0, p1, p2, p3, tm, hermiteBasisMat));
        if (tp > 0.0f && tp < 1.0f)
            aabb.expand(interpolateCubic(p0, p1, p2, p3, tp, hermiteBasisMat));
    }

    return aabb;
}
