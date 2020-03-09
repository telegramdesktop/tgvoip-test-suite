#pragma once

#include <vector>
#include <memory>
#include <functional>
#include <algorithm>
#include <cassert>

namespace tgvoiprate {


class VectorOfColumns
{
public:
    using Iterator = std::vector<double>::iterator;

    VectorOfColumns(const std::vector<double>& data, size_t rowsCount)
        : mRowsCount{rowsCount}
        , mData(data)
    {}

    VectorOfColumns(Iterator begin, Iterator end, size_t rowsCount)
        : mRowsCount{rowsCount}
        , mData(begin, end)
    {}

    VectorOfColumns(size_t rowsCount, size_t length = 0)
        : mRowsCount{rowsCount}
        , mData(rowsCount * length)
    {}

    double& At(int column, int row)
    {
        return mData[column * RowsCount() + row];
    };

    size_t RowsCount() const
    {
        return mRowsCount;
    }

    size_t Length() const
    {
        return (mData.size()) / RowsCount();
    }

    double Accumulate(double startValue, std::function<double(double, double)> accFunction) const
    {
        double result = startValue;
        for (double x: mData)
        {
            result = accFunction(result, x);
        }
        return result;
    }

    double Min() const
    {
        return *std::min_element(mData.begin(), mData.end());
    }

    double Max() const
    {
        return *std::max_element(mData.begin(), mData.end());
    }

    double Mean() const
    {
        return Accumulate(0, [this](double result, double value){ return result + value / mData.size(); });
    }

    VectorOfColumns& ForEach(std::function<void(double&)> func)
    {
        for (double& x: mData)
        {
            func(x);
        }
        return *this;
    }

    VectorOfColumns& ForEachIndexed(std::function<void(int, int, double&)> func)
    {
        for (size_t i = 0; i < mData.size(); ++i)
        {
            func(i / RowsCount(), i % RowsCount(), mData[i]);
        }
        return *this;
    }

    void ForEachFrame(int frameLength, int step, std::function<void(int, VectorOfColumns&)> func)
    {
        for (int i = 0; i + frameLength <= Length(); i += step)
        {
            VectorOfColumns frame = SubCopy(i, frameLength);
            func(i, frame);
        }
    }

    VectorOfColumns& CombineWith(VectorOfColumns& another, std::function<double(double, double)> op)
    {
        assert(another.Length() == Length());
        assert(another.RowsCount() == RowsCount());
        return ForEachIndexed([&another, op](int column, int row, double& value)
        {
            value = op(value, another.At(column, row));
        });
    }

    VectorOfColumns operator*(VectorOfColumns& another)
    {
        return SubCopy().CombineWith(another, [](double a, double b){ return a * b; });
    }

    VectorOfColumns operator/(VectorOfColumns& another)
    {
        return SubCopy().CombineWith(another, [](double a, double b){ return a / b; });
    }

    VectorOfColumns operator+(VectorOfColumns& another)
    {
        return SubCopy().CombineWith(another, [](double a, double b){ return a + b; });
    }

    VectorOfColumns operator-(VectorOfColumns& another)
    {
        return SubCopy().CombineWith(another, [](double a, double b){ return a - b; });
    }

    VectorOfColumns Convolve(VectorOfColumns& window)
    {
        assert(window.RowsCount() <= RowsCount());
        assert(window.Length() <= Length());
        assert(window.RowsCount() % 2 == 1);
        assert(window.Length() % 2 == 1);

        VectorOfColumns result{RowsCount() - window.RowsCount() - 1, Length() - window.Length() - 1};
        ForEachIndexed([&](int column, int row, double& value){
            int dstColumn = column - window.Length() / 2;
            int dstRow = row - window.RowsCount() / 2;
            if (dstColumn < 0 || dstColumn >= result.Length()) { return; }
            if (dstRow < 0 || dstRow >= result.RowsCount()) { return; }

            window.ForEachIndexed([&](int wnd_column, int wnd_row, double& wnd_value)
            {
                int srcColumn = column + wnd_column - window.Length() / 2;
                int srcRow = row + wnd_row - window.RowsCount() / 2;
                result.At(dstColumn, dstRow) += wnd_value * this->At(srcColumn, srcRow);
            });
        });

        return result;
    }

    void Append(const std::vector<double>& column)
    {
        assert(column.size() == RowsCount());
        mData.insert(mData.end(), column.begin(), column.end());
    }

    VectorOfColumns SubCopy(size_t startColumn = 0, size_t length = 0)
    {
        assert(startColumn + length <= Length());
        if (length == 0)
        {
            length = Length() - startColumn;
        }
        return VectorOfColumns {
            mData.begin() + startColumn * RowsCount(),
            mData.begin() + (startColumn + length) * RowsCount(),
            RowsCount()
        };
    }

private:

    size_t mRowsCount;
    std::vector<double> mData;
};

}
