class OctobrewTui < Formula
  desc "Terminal UI for Homebrew with the tabs, keys and themes of OctoPkg"
  homepage "https://github.com/cleberalves40/octobrew-tui"
  url "https://github.com/cleberalves40/octobrew-tui/archive/refs/tags/v1.0.0.tar.gz"
  sha256 "0bfefdbef4fe33c7a9590d86c294b1bbb940e210aee1d02a858f3b2d68d1be36"
  license "GPL-2.0-or-later"
  head "https://github.com/cleberalves40/octobrew-tui.git", branch: "main"

  depends_on "cmake" => :build
  depends_on "ninja" => :build
  depends_on "ftxui"

  def install
    system "cmake", "-S", ".", "-B", "build", "-G", "Ninja", *std_cmake_args
    system "cmake", "--build", "build"
    system "cmake", "--install", "build"
  end

  test do
    assert_match "octobrew-tui #{version}", shell_output("#{bin}/octobrew-tui --version")
    assert_match "Homebrew", shell_output("#{bin}/octobrew-tui --help")
    # Uma opção desconhecida sai com 2, sem abrir a tela.
    assert_match "--remote", shell_output("#{bin}/octobrew-tui --nao-existe 2>&1", 2)
  end
end
