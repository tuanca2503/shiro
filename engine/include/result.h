#pragma once

#include <string>

namespace shiro
{
    struct Result
    {
        bool success = false;
        std::string text = "";

        static Result ok(std::string text = "")
        {
            return Result{true, std::move(text)};
        }

        static Result failed(std::string error_msg)
        {
            return Result{false, std::move(error_msg)};
        }
    };
}