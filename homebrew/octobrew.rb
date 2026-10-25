class Octobrew < Formula
  desc "Graphical front end for Homebrew with the look of OctoPkg"
  homepage "https://github.com/cleberalves40/octobrew"
  url "https://github.com/cleberalves40/octobrew/archive/refs/tags/v1.0.0.tar.gz"
  sha256 "04d9b51069218b05d01146e03a240cadd8301719eb908805b61d8335dd58436c"
  license "GPL-2.0-or-later"
  head "https://github.com/cleberalves40/octobrew.git", branch: "main"

  depends_on "cmake" => :build
  depends_on "ninja" => :build
  depends_on "qtbase"

  def install
    system "cmake", "-S", ".", "-B", "build", "-G", "Ninja", *std_cmake_args
    system "cmake", "--build", "build"
    system "cmake", "--install", "build"
    # On macOS, OctoBrew.app (with octobrew-askpass and octobrew-notifier
    # inside) stays in the prefix; the octobrew command opens it from a shell.
    bin.write_exec_script prefix/"OctoBrew.app/Contents/MacOS/OctoBrew" if OS.mac?
  end

  def caveats
    on_macos do
      <<~EOS
        OctoBrew.app was installed to:
          #{opt_prefix}/OctoBrew.app

        To have it in Launchpad and the Finder:
          ln -sf #{opt_prefix}/OctoBrew.app /Applications/OctoBrew.app
      EOS
    end
  end

  test do
    # No display: the offscreen plugin of qtbase.
    ENV["QT_QPA_PLATFORM"] = "offscreen"
    app = OS.mac? ? prefix/"OctoBrew.app/Contents/MacOS" : bin
    main = OS.mac? ? app/"OctoBrew" : bin/"octobrew"
    assert_match "OctoBrew #{version}", shell_output("#{main} --version")
    assert_match "Homebrew", shell_output("#{main} --help")
    # The sudo password dialog and the menu bar notifier come along.
    assert_path_exists app/"octobrew-askpass"
    assert_match "OctoBrew #{version}", shell_output("#{app}/octobrew-notifier --version")
    assert_match "OctoBrew #{version}", shell_output("#{bin}/octobrew --version") if OS.mac?
  end
end
