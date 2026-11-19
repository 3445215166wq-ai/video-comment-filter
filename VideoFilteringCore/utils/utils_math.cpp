#include "utils.h"
#include <cmath>
#include <stdio.h>

/**
 * @file utils_math.cpp
 * @brief 数学工具函数：向量相似度计算
 */

namespace Utils
{

    /**
     * @brief 计算两个向量的余弦相似度
     * @param vec1 第一个特征向量
     * @param vec2 第二个特征向量
     * @return 余弦相似度 [-1, 1]，计算失败返回 0
     */
    float calc_similarity(const std::vector<float>& vec1, const std::vector<float>& vec2)
    {
        if (vec1.empty() || vec2.empty() || vec1.size() != vec2.size())
        {
            printf("[错误] 向量相似度计算失败：向量为空或长度不匹配 (vec1=%d, vec2=%d)\n",
                (int)vec1.size(), (int)vec2.size());
            return 0.0f;
        }

        float dot_product = 0.0f;
        float norm1 = 0.0f;
        float norm2 = 0.0f;

        for (size_t i = 0; i < vec1.size(); i++)
        {
            dot_product += vec1[i] * vec2[i];
            norm1 += vec1[i] * vec1[i];
            norm2 += vec2[i] * vec2[i];
        }

        if (norm1 < 1e-6f || norm2 < 1e-6f)
        {
            printf("[错误] 向量模长为0，无法计算相似度\n");
            return 0.0f;
        }

        // 余弦相似度 = 点积 / (模A * 模B)
        float similarity = dot_product / ((float)sqrt(norm1) * (float)sqrt(norm2));

        DebugLog("[相似度] 点积=%.6f, 模1=%.6f, 模2=%.6f, 结果=%.6f",
            dot_product, norm1, norm2, similarity);

        // 值域限制在 [-1, 1]
        if (similarity < -1.0f) similarity = -1.0f;
        if (similarity > 1.0f) similarity = 1.0f;

        return similarity;
    }

}