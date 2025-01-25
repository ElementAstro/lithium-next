#pragma once

#include <QObject>
#include <libstellarsolver/stellarsolver.h>

class SolverConfig {
public:
    static SolverConfig& getInstance();

    // 快速配置预设
    void setQuickMode();      // 快速模式,精度较低
    void setPreciseMode();    // 精确模式,耗时较长
    void setBalancedMode();   // 平衡模式

    // 自定义配置
    void setCustomParameters(const SSolver::Parameters& params);

    // 获取当前配置
    const SSolver::Parameters& getParameters() const;

    // 保存/加载配置
    bool saveToFile(const QString& filename);
    bool loadFromFile(const QString& filename);

private:
    SolverConfig();
    SSolver::Parameters currentParams_;
};
