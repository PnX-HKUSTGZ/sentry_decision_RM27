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
    ros-jazzy-rosbridge-suite \
    libyaml-cpp-dev \
    libboost-dev \
    libfmt-dev \
    && rm -rf /var/lib/apt/lists/*

RUN rosdep init || true

RUN echo 'source /opt/ros/jazzy/setup.bash' >> /etc/bash.bashrc

# 创建与宿主同 UID 的开发用户，避免 Dev Container / 容器写入的文件归属混乱。
ARG USER_UID=1001
ARG USER_GID=1001
RUN if ! getent group ${USER_GID} >/dev/null; then groupadd --gid ${USER_GID} dev; fi \
    && if ! id -u ${USER_UID} >/dev/null 2>&1; then useradd --uid ${USER_UID} --gid ${USER_GID} --create-home --shell /bin/bash dev; fi \
    && echo 'dev ALL=(ALL) NOPASSWD:ALL' >> /etc/sudoers

USER dev

WORKDIR /ws
