# sentry_decision_RM27 开发 / 测试镜像
# Ubuntu 24.04 / ROS 2 Jazzy，预装构建决策工作区所需的系统与 ROS 依赖。
# 构建：docker build -f docker/Dockerfile -t sentry_decision_rm27:jazzy .
FROM osrf/ros:jazzy-desktop

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y --no-install-recommends \
    git \
    python3-colcon-common-extensions \
    python3-rosdep \
    && rm -rf /var/lib/apt/lists/*

RUN apt-get update && apt-get install -y --no-install-recommends \
    ros-jazzy-behaviortree-cpp \
    ros-jazzy-nav2-msgs \
    libyaml-cpp-dev \
    libboost-dev \
    libfmt-dev \
    && rm -rf /var/lib/apt/lists/*

RUN rosdep init || true

RUN echo 'source /opt/ros/jazzy/setup.bash' >> /etc/bash.bashrc

WORKDIR /ws
