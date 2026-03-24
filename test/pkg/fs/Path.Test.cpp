#include "Fs/Path.h"

#include <filesystem>
#include <gtest/gtest.h>

using Fs::Path;

// --- filename_view ---

TEST(PathView, FilenameViewNormal)
{
    Path p("/home/user/file.txt");
    EXPECT_EQ(p.filename_view(), "file.txt");
}

TEST(PathView, FilenameViewNoDirectory)
{
    Path p("file.txt");
    EXPECT_EQ(p.filename_view(), "file.txt");
}

TEST(PathView, FilenameViewTrailingSlash)
{
    Path p("/home/user/dir/");
    EXPECT_EQ(p.filename_view(), "dir");
}

TEST(PathView, FilenameViewRoot)
{
    Path p("/");
    // std::filesystem::path("/").filename() is empty — root has no filename
    EXPECT_EQ(p.filename_view(), "");
}

TEST(PathView, FilenameViewEmpty)
{
    Path p;
    EXPECT_EQ(p.filename_view(), "");
}

TEST(PathView, FilenameViewDot)
{
    Path p(".");
    EXPECT_EQ(p.filename_view(), ".");
}

TEST(PathView, FilenameViewDotDot)
{
    Path p("/home/user/..");
    EXPECT_EQ(p.filename_view(), "..");
}

// --- stem_view ---

TEST(PathView, StemViewNormal)
{
    Path p("/home/user/file.txt");
    EXPECT_EQ(p.stem_view(), "file");
}

TEST(PathView, StemViewNoExtension)
{
    Path p("/home/user/Makefile");
    EXPECT_EQ(p.stem_view(), "Makefile");
}

TEST(PathView, StemViewDotfile)
{
    Path p("/home/user/.bashrc");
    EXPECT_EQ(p.stem_view(), ".bashrc");
}

TEST(PathView, StemViewMultipleDots)
{
    Path p("/home/user/archive.tar.gz");
    EXPECT_EQ(p.stem_view(), "archive.tar");
}

TEST(PathView, StemViewEmpty)
{
    Path p;
    EXPECT_EQ(p.stem_view(), "");
}

// --- extension_view ---

TEST(PathView, ExtensionViewNormal)
{
    Path p("/home/user/file.txt");
    EXPECT_EQ(p.extension_view(), ".txt");
}

TEST(PathView, ExtensionViewNoExtension)
{
    Path p("/home/user/Makefile");
    EXPECT_EQ(p.extension_view(), "");
}

TEST(PathView, ExtensionViewDotfile)
{
    Path p("/home/user/.bashrc");
    EXPECT_EQ(p.extension_view(), "");
}

TEST(PathView, ExtensionViewMultipleDots)
{
    Path p("/home/user/archive.tar.gz");
    EXPECT_EQ(p.extension_view(), ".gz");
}

TEST(PathView, ExtensionViewEmpty)
{
    Path p;
    EXPECT_EQ(p.extension_view(), "");
}

// --- parent_path_view ---

TEST(PathView, ParentPathViewNormal)
{
    Path p("/home/user/file.txt");
    EXPECT_EQ(p.parent_path_view(), "/home/user");
}

TEST(PathView, ParentPathViewRoot)
{
    Path p("/");
    EXPECT_EQ(p.parent_path_view(), "/");
}

TEST(PathView, ParentPathViewNoParent)
{
    Path p("file.txt");
    EXPECT_EQ(p.parent_path_view(), "");
}

TEST(PathView, ParentPathViewTrailingSlash)
{
    Path p("/home/user/dir/");
    EXPECT_EQ(p.parent_path_view(), "/home/user");
}

TEST(PathView, ParentPathViewEmpty)
{
    Path p;
    EXPECT_EQ(p.parent_path_view(), "");
}

TEST(PathView, ParentPathViewNested)
{
    Path p("/a/b/c/d");
    EXPECT_EQ(p.parent_path_view(), "/a/b/c");
}

// --- Zero-allocation proof: views point into native() storage ---

TEST(PathView, ViewsPointIntoNativeStorage)
{
    Path p("/home/user/file.txt");
    const auto& native = p.native();
    auto fn = p.filename_view();
    auto st = p.stem_view();
    auto ex = p.extension_view();
    auto pp = p.parent_path_view();

    // All views should point into the native string's buffer
    EXPECT_GE(fn.data(), native.data());
    EXPECT_LE(fn.data() + fn.size(), native.data() + native.size());

    EXPECT_GE(st.data(), native.data());
    EXPECT_LE(st.data() + st.size(), native.data() + native.size());

    EXPECT_GE(ex.data(), native.data());
    EXPECT_LE(ex.data() + ex.size(), native.data() + native.size());

    EXPECT_GE(pp.data(), native.data());
    EXPECT_LE(pp.data() + pp.size(), native.data() + native.size());
}

// --- Implicit conversion from std::filesystem::path ---

TEST(PathView, ConstructFromStdPath)
{
    std::filesystem::path stdPath("/some/path/file.cpp");
    Path p = stdPath;
    EXPECT_EQ(p.filename_view(), "file.cpp");
    EXPECT_EQ(p.extension_view(), ".cpp");
}

TEST(PathView, AssignFromStdPath)
{
    std::filesystem::path stdPath("/another/path.h");
    Path p;
    p = stdPath;
    EXPECT_EQ(p.stem_view(), "path");
    EXPECT_EQ(p.parent_path_view(), "/another");
}

TEST(PathView, MoveFromStdPath)
{
    std::filesystem::path stdPath("/move/test.dat");
    Path p = std::move(stdPath);
    EXPECT_EQ(p.filename_view(), "test.dat");
}
