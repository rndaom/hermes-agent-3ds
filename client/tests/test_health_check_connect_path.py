from pathlib import Path


CLIENT_DIR = Path(__file__).resolve().parents[1]


def test_health_check_does_not_depend_on_so_error_after_connect_wait():
    source = (CLIENT_DIR / "source" / "bridge_health.c").read_text()
    assert "wait_for_socket(socket_fd, true" in source
    assert "getsockopt(socket_fd, SOL_SOCKET, SO_ERROR" not in source
