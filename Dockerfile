FROM osrf/ros:jazzy-desktop-full

ENV DEBIAN_FRONTEND=noninteractive

# Install core development tools
RUN apt-get update && apt-get install -y \
    python3-colcon-common-extensions \
    build-essential \
    git \
    sudo \
    && rm -rf /var/lib/apt/lists/*

# Fix permission issues for non-root users across different host OS environments
ARG USERNAME=sra
ARG USER_UID=1000
ARG USER_GID=$USER_UID

# If GID 1000 already exists, rename it/modify it; otherwise, create it fresh
RUN if getent group $USER_GID; then \
    existing_group=$(getent group $USER_GID | cut -d: -f1); \
    groupmod -n $USERNAME $existing_group; \
    else \
    groupadd --gid $USER_GID $USERNAME; \
    fi \
    && if getent passwd $USER_UID; then \
    existing_user=$(getent passwd $USER_UID | cut -d: -f1); \
    usermod -l $USERNAME -m -d /home/$USERNAME $existing_user; \
    else \
    useradd --uid $USER_UID --gid $USER_GID -m $USERNAME; \
    fi \
    && echo $USERNAME ALL=\(root\) NOPASSWD:ALL > /etc/sudoers.d/$USERNAME \
    && chmod 0440 /etc/sudoers.d/$USERNAME


COPY entrypoint.sh /entrypoint.sh
RUN chmod +x /entrypoint.sh
CMD ["/entrypoint.sh"]
# CMD ["bash", "ros2 wtf"]
# CMD ["bash", "sudo apt update"]
# CMD ["bash", "sudo apt upgrade"]
