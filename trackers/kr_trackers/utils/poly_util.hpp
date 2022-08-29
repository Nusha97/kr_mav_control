/*
    MIT License
    Copyright (c) 2020 Zhepei Wang (wangzhepei@live.com)
    Permission is hereby granted, free of charge, to any person obtaining a copy
    of this software and associated documentation files (the "Software"), to deal
    in the Software without restriction, including without limitation the rights
    to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
    copies of the Software, and to permit persons to whom the Software is
    furnished to do so, subject to the following conditions:
    The above copyright notice and this permission notice shall be included in all
    copies or substantial portions of the Software.
    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
    IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
    FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
    AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
    LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
    OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
    SOFTWARE.
*/


// revise to only generate minimum jerk trajs

#ifndef POLY_UTIL_HPP
#define POLY_UTIL_HPP

#include <Eigen/Eigen>
#include <iostream>
#include <cmath>
#include <vector>


namespace min_jerk
{
    // Polynomial order and trajectory dimension are fixed here
    constexpr int TrajOrder = 5;
    constexpr int TrajDim = 3;

    // Type for piece boundary condition and coefficient matrix
    typedef Eigen::Matrix<double, TrajDim, TrajOrder + 1> BoundaryCond;
    typedef Eigen::Matrix<double, TrajDim, TrajOrder + 1> CoefficientMat;
    typedef Eigen::Matrix<double, TrajDim, TrajOrder> VelCoefficientMat;
    typedef Eigen::Matrix<double, TrajDim, TrajOrder - 1> AccCoefficientMat;

    // A single piece of a trajectory, which is indeed a polynomial
    class Piece
    {
    private:
        // A piece is totally determined by boundary condition and duration
        // boundCond = [p(0),v(0),a(0),p(T),v(T),a(T)]
        BoundaryCond boundCond;
        double duration;
        bool synced;
        // The normalized coefficient is generated from boundCond and duration
        // These members CANNOT be accessed unless through normalizedCoeffMat()
        // p(t) = c5*t^5 + c4*t^4 + ... + c1*t + c0
        // nCoeffMat = [c5*T^5,c4*T^4,c3*T^3,c2*T^2,c1*T,c0*1]
        CoefficientMat nCoeffMat;
        inline void normalizedCoeffMat(void)
        {
            if(!synced){

                double t1 = duration;
                double t2 = t1 * t1;

                // It maps boundary condition to normalized coefficient matrix
                nCoeffMat.col(0) = 0.5 * (boundCond.col(5) - boundCond.col(2)) * t2 -
                                    3.0 * (boundCond.col(1) + boundCond.col(4)) * t1 +
                                    6.0 * (boundCond.col(3) - boundCond.col(0));
                nCoeffMat.col(1) = (-boundCond.col(5) + 1.5 * boundCond.col(2)) * t2 +
                                    (8.0 * boundCond.col(1) + 7.0 * boundCond.col(4)) * t1 +
                                    15.0 * (-boundCond.col(3) + boundCond.col(0));
                nCoeffMat.col(2) = (0.5 * boundCond.col(5) - 1.5 * boundCond.col(2)) * t2 -
                                    (6.0 * boundCond.col(1) + 4.0 * boundCond.col(4)) * t1 +
                                    10.0 * (boundCond.col(3) - boundCond.col(0));
                nCoeffMat.col(3) = 0.5 * boundCond.col(2) * t2;
                nCoeffMat.col(4) = boundCond.col(1) * t1;
                nCoeffMat.col(5) = boundCond.col(0);
            }

            return;
        }

    public:
        Piece() = default;

        // Constructor from boundary condition and duration
        Piece(BoundaryCond bdCond, double dur) : boundCond(bdCond), duration(dur), synced(false) {
          normalizedCoeffMat();
        }

        inline int getDim() const
        {
            return TrajDim;
        }

        inline int getOrder() const
        {
            return TrajOrder;
        }

        inline double getDuration() const
        {
            return duration;
        }

        // Get the position at time t in this piece
        inline Eigen::Vector3d getPos(double t)
        {
            // Normalize the time
            t /= duration;
            Eigen::Vector3d pos(0.0, 0.0, 0.0);
            double tn = 1.0;
            for (int i = TrajOrder; i >= 0; i--)
            {
                pos += tn * nCoeffMat.col(i);
                tn *= t;
            }
            // The pos is not affected by normalization
            return pos;
        }

        // Get the velocity at time t in this piece
        inline Eigen::Vector3d getVel(double t)
        {
            // Normalize the time
            t /= duration;
            Eigen::Vector3d vel(0.0, 0.0, 0.0);
            double tn = 1.0;
            int n = 1;
            for (int i = TrajOrder - 1; i >= 0; i--)
            {
                vel += n * tn * nCoeffMat.col(i);
                tn *= t;
                n++;
            }
            // Recover the actual vel
            vel /= duration;
            return vel;
        }

        // Get the acceleration at time t in this piece
        inline Eigen::Vector3d getAcc(double t)
        {
            // Normalize the time
            t /= duration;
            Eigen::Vector3d acc(0.0, 0.0, 0.0);
            double tn = 1.0;
            int m = 1;
            int n = 2;
            for (int i = TrajOrder - 2; i >= 0; i--)
            {
                acc += m * n * tn * nCoeffMat.col(i);
                tn *= t;
                m++;
                n++;
            }
            // Recover the actual acc
            acc /= duration * duration;
            return acc;
        }

        inline Eigen::Vector3d getJer(double t)
        {
            t /= duration;
            Eigen::Vector3d jer(0.0, 0.0, 0.0);
            double tn = 1.0;
            int l = 1;
            int m = 2;
            int n = 3;
            for (int i = TrajOrder - 3; i >= 0; i--)
            {
                jer += l * m * n * tn * nCoeffMat.col(i);
                tn *= t;
                l++;
                m++;
                n++;
            }
            jer /= duration * duration * duration;
            return jer;
        }


    };

    // A whole trajectory which contains multiple pieces
    class Trajectory
    {
    private:
        typedef std::vector<Piece> Pieces;
        Pieces pieces;

    public:
        Trajectory() = default;

        // Constructor from boundary conditions and durations
        Trajectory(const std::vector<BoundaryCond> &bdConds,
                   const std::vector<double> &durs)
        {
            int N = std::min(durs.size(), bdConds.size());
            pieces.reserve(N);
            for (int i = 0; i < N; i++)
            {
                pieces.emplace_back(bdConds[i], durs[i]);
            }
        }

        inline int getPieceNum() const
        {
            return pieces.size();
        }

        // Get durations vector of all pieces
        inline Eigen::VectorXd getDurations() const
        {
            int N = getPieceNum();
            Eigen::VectorXd durations(N);
            for (int i = 0; i < N; i++)
            {
                durations(i) = pieces[i].getDuration();
            }
            return durations;
        }

        // Get total duration of the trajectory
        inline double getTotalDuration() const
        {
            int N = getPieceNum();
            double totalDuration = 0.0;
            for (int i = 0; i < N; i++)
            {
                totalDuration += pieces[i].getDuration();
            }
            return totalDuration;
        }

        // Reload the operator[] to access the i-th piece
        inline const Piece &operator[](int i) const
        {
            return pieces[i];
        }

        inline Piece &operator[](int i)
        {
            return pieces[i];
        }

        inline void clear(void)
        {
            pieces.clear();
            return;
        }

        inline Pieces::const_iterator begin() const
        {
            return pieces.begin();
        }

        inline Pieces::const_iterator end() const
        {
            return pieces.end();
        }

        inline Pieces::iterator begin()
        {
            return pieces.begin();
        }

        inline Pieces::iterator end()
        {
            return pieces.end();
        }

        inline void reserve(const int &n)
        {
            pieces.reserve(n);
            return;
        }

        // Put another piece at the tail of this trajectory
        inline void emplace_back(const Piece &piece)
        {
            pieces.emplace_back(piece);
            return;
        }

        inline void emplace_back(const BoundaryCond &bdCond, const double &dur)
        {
            pieces.emplace_back(bdCond, dur);
            return;
        }

        // Find the piece at which the time t is located
        // The index is returned and the offset in t is removed
        inline int locatePieceIdx(double &t) const
        {
            int N = getPieceNum();
            int idx;
            double dur;
            for (idx = 0;
                 idx < N &&
                 t > (dur = pieces[idx].getDuration());
                 idx++)
            {
                t -= dur;
            }
            if (idx == N)
            {
                idx--;
                t += pieces[idx].getDuration();
            }
            return idx;
        }

        // Get the position at time t of the trajectory
        inline Eigen::Vector3d getPos(double t)
        {
            int pieceIdx = locatePieceIdx(t);
            return pieces[pieceIdx].getPos(t);
        }

        // Get the velocity at time t of the trajectory
        inline Eigen::Vector3d getVel(double t)
        {
            int pieceIdx = locatePieceIdx(t);
            return pieces[pieceIdx].getVel(t);
        }

        // Get the acceleration at time t of the trajectory
        inline Eigen::Vector3d getAcc(double t)
        {
            int pieceIdx = locatePieceIdx(t);
            return pieces[pieceIdx].getAcc(t);
        }

        // Get the jerk at time t of the trajectory
        inline Eigen::Vector3d getJer(double t)
        {
            int pieceIdx = locatePieceIdx(t);
            return pieces[pieceIdx].getJer(t);
        }

    };

} // namespace min_jerk

#endif