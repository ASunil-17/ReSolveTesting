#pragma once
#include <string>
#include <vector>
#include <sstream>
#include <iterator>
#include <algorithm>
#include <resolve/matrix/Csr.hpp>
#include <resolve/matrix/Csc.hpp>
#include <resolve/workspace/LinAlgWorkspace.hpp>
#include <resolve/matrix/MatrixHandler.hpp>
#include <resolve/vector/Vector.hpp>
#include <tests/unit/TestBase.hpp>

namespace ReSolve { namespace tests {

/**
 * @class Unit tests for matrix handler class
 */
class MatrixHandlerTests : TestBase
{
public:
  MatrixHandlerTests(ReSolve::MatrixHandler& handler) : handler_(handler)
  {
    if (handler_.getIsCudaEnabled() || handler_.getIsHipEnabled()) {
      memspace_ = memory::DEVICE;
    } else {
      memspace_ = memory::HOST;
    }
  }

  virtual ~MatrixHandlerTests()
  {}

  TestOutcome matrixHandlerConstructor()
  {
    TestStatus status;
    status.skipTest();

    return status.report(__func__);
  }

  TestOutcome matrixInfNorm(index_type n)
  {
    TestStatus status;

    matrix::Csr* A = createCsrMatrix(n);
    real_type norm;
    handler_.matrixInfNorm(A, &norm, memspace_);
    status *= (norm == 30.0);

    delete A;

    return status.report(__func__);
  }

  TestOutcome matVec(index_type n)
  {
    TestStatus status;

    matrix::Csr* A = createCsrMatrix(n);
    vector::Vector x(n);
    vector::Vector y(n);
    x.allocate(memspace_);
    if (x.getData(memspace_) == NULL)
      std::cout << "The memory space was not allocated \n" << std::endl;
    y.allocate(memspace_);

    x.setToConst(1.0, memspace_);
    y.setToConst(1.0, memspace_);

    real_type alpha = 2.0/30.0;
    real_type beta  = 2.0;
    handler_.setValuesChanged(true, memspace_);
    handler_.matvec(A, &x, &y, &alpha, &beta, memspace_);

    status *= verifyAnswer(y, 4.0);

    delete A;

    return status.report(__func__);
  }

  TestOutcome csc2csr(index_type n, index_type m)
  {
    TestStatus status;

    std::string testname(__func__);
    std::stringstream matrix_size;
    matrix_size << " for " << m << " x " << n << " matrix";
    testname += matrix_size.str();

    matrix::Csc* A_csc = createRectangularCscMatrix(n, m);
    matrix::Csr* A_csr = new matrix::Csr(m, n, A_csc->getNnz());
    A_csr->allocateMatrixData(memspace_);

    handler_.csc2csr(A_csc, A_csr, memspace_);

    status *= (A_csr->getNumRows() == A_csc->getNumRows());
    status *= (A_csr->getNumColumns() == A_csc->getNumColumns());
    status *= (A_csr->getNnz() == A_csc->getNnz());

    // Move data to the host for the result verification
    if (memspace_ == memory::DEVICE) {
      A_csr->syncData(memory::HOST);
    }

    verifyCsrMatrix(A_csr);

    delete A_csr;
    delete A_csc;

    return status.report(testname.c_str());
  }

  TestOutcome transpose(index_type n, index_type m)
  {
      TestStatus status;
      std::string testname(__func__);
      std::stringstream matrix_size;
      matrix_size << " for " << n << " x " << m << " matrix";
      testname += matrix_size.str();

      matrix::Csr* At = new matrix::Csr(m, n, 2 * std::min(n, m));
      matrix::Csr* A = nullptr;  // Declare A outside

      for (real_type val = 0.0; val <= 1.0; val += 1.0) {  // Use a step to prevent infinite loop
          if (val == 0.0) {
              A = createRectangularCsrMatrix(n, m);
              At->allocateMatrixData(memspace_);
              handler_.transpose(A, At, memspace_);

              status *= (At->getNumRows() == A->getNumColumns());
              status *= (At->getNumColumns() == A->getNumRows());
              status *= (At->getNnz() == A->getNnz());
          } else {
              handler_.addConst(A, val, memspace_);
              handler_.transpose(A, At, memspace_);
          }

          if (memspace_ == memory::DEVICE) {
              At->syncData(memory::HOST);
          }

          verifyCsrMatrix(At, val);
      }

      delete A;  // Delete after loop
      delete At;

      return status.report(testname.c_str());
  }

private:
  ReSolve::MatrixHandler& handler_;
  memory::MemorySpace memspace_{memory::HOST};

  bool verifyAnswer(vector::Vector& x, real_type answer)
  {
    bool status = true;
    if (memspace_ == memory::DEVICE) {
      x.syncData(memory::HOST);
    }

    for (index_type i = 0; i < x.getSize(); ++i) {
      if (!isEqual(x.getData(memory::HOST)[i], answer)) {
        status = false;
        std::cout << "Solution vector element x[" << i << "] = " << x.getData(memory::HOST)[i]
                  << ", expected: " << answer << "\n";
        break;
      }
    }
    return status;
  }

  /**
   * @brief Create a rectangular CSC matrix with preset sparsity structure
   *
   * The sparisty structure is upper bidiagonal if n==m, with an extra entry in the first column
   * If n>m A_{ij} is nonzero iff i==j, or i+m==j+n
   * if n<m A_{ij} is nonzero iff i==j, or i+m==j+n
   * The values increase with a counter from 1.0 in column major order.
   *
   * @param[in] n number of columns
   * @param[in] m number of rows
   *
   * @return matrix::Csc*
   */
  matrix::Csc* createRectangularCscMatrix(const index_type n, const index_type m)
  {
    index_type nnz = 2 * std::min(n, m);
    matrix::Csc* A = new matrix::Csc(m, n, nnz);
    A->allocateMatrixData(memory::HOST);

    index_type* colptr = A->getColData(memory::HOST);
    index_type* rowidx = A->getRowData(memory::HOST);
    real_type* val = A->getValues(memory::HOST);

    real_type counter = 1.0;
    colptr[0] = 0;
    if (n == m) {
      for (index_type i = 0; i < n; ++i) {
        colptr[i + 1] = colptr[i] + 2;
        if (i == 0) {
          rowidx[colptr[i]] = i;
          val[colptr[i]] = counter++;
          rowidx[colptr[i] + 1] = m / 2;
          val[colptr[i] + 1] = counter++;
        } else {
          rowidx[colptr[i]] = i - 1;
          val[colptr[i]] = counter++;
          rowidx[colptr[i] + 1] = i;
          val[colptr[i] + 1] = counter++;
        }
      }
    } else if (n > m) {
      for (index_type i = 0; i < n; ++i) {
        colptr[i + 1] = colptr[i];
        if (i >= n - m) {
          rowidx[colptr[i + 1]] = i - n + m;
          val[colptr[i + 1]] = counter++;
          colptr[i + 1]++;
        }
        if (i < m) {
          rowidx[colptr[i + 1]] = i;
          val[colptr[i + 1]] = counter++;
          colptr[i + 1]++;
        }
      }
    } else {
      for (index_type i = 0; i < n; ++i) {
        colptr[i + 1] = colptr[i] + 2;
        rowidx[colptr[i]] = i;
        val[colptr[i]] = counter++;
        rowidx[colptr[i] + 1] = i + m - n;
        val[colptr[i] + 1] = counter++;
      }
    }
    A->setUpdated(memory::HOST);
    if (memspace_ == memory::DEVICE) {
      A->syncData(memspace_);
    }
    return A;
  }

  /**
   * @brief Create a rectangular CSR matrix with preset sparsity structure
   *
   * The sparisty structure is lower bidiagonal if n==m, with an extra entry in the first row
   * If n>m A_{ij} is nonzero iff i==j, or i+m==j+n
   * if n<m A_{ij} is nonzero iff i==j, or i+m==j+n
   *
   * The values increase with a counter from 1.0 in row major order.
   *
   * @param[in] n number of rows
   * @param[in] m number of columns
   *
   * @return matrix::Csr*
   */

  matrix::Csr* createRectangularCsrMatrix(const index_type n, const index_type m)
  {
    index_type nnz = 2 * std::min(n, m);
    matrix::Csr* A = new matrix::Csr(n, m, nnz);
    A->allocateMatrixData(memory::HOST);

    index_type* rowptr = A->getRowData(memory::HOST);
    index_type* colidx = A->getColData(memory::HOST);
    real_type* val = A->getValues(memory::HOST);

    real_type counter = 1.0;
    rowptr[0] = 0;
    if (n == m) {
      for (index_type i = 0; i < n; ++i) {
        rowptr[i + 1] = rowptr[i] + 2;
        if (i == 0) {
          colidx[rowptr[i]] = i;
          val[rowptr[i]] = counter++;
          colidx[rowptr[i] + 1] = n / 2;
          val[rowptr[i] + 1] = counter++;
        } else {
          colidx[rowptr[i]] = i - 1;
          val[rowptr[i]] = counter++;
          colidx[rowptr[i] + 1] = i;
          val[rowptr[i] + 1] = counter++;
        }
      }
    } else if (n > m) {
      for (index_type i = 0; i < n; ++i) {
        rowptr[i + 1] = rowptr[i];
        if (i >= n - m) {
          colidx[rowptr[i + 1]] = i - n + m;
          val[rowptr[i + 1]] = counter++;
          rowptr[i + 1]++;
        }
        if (i < m) {
          colidx[rowptr[i + 1]] = i;
          val[rowptr[i + 1]] = counter++;
          rowptr[i + 1]++;
        }
      }
    } else {
      for (index_type i = 0; i < n; ++i) {
        rowptr[i + 1] = rowptr[i] + 2;
        colidx[rowptr[i]] = i;
        val[rowptr[i]] = counter++;
        colidx[rowptr[i] + 1] = i + m - n;
        val[rowptr[i] + 1] = counter++;
      }
    }

    A->setUpdated(memory::HOST);
    if (memspace_ == memory::DEVICE) {
      A->syncData(memspace_);
    }
    return A;
  }

  /**
   * @brief Verify structure of a CSR matrix with preset pattern.
   *
   * The sparsity structure corresponds to the CSR representation of a rectangular matrix
   * created by createRectangularCscMatrix.
   * The sparisty structure is upper bidiagonal if n==m,
   * with an extra entry in the first column.
   * If n>m A_{ij} is nonzero iff i==j, or i+m==j+n
   * if n<m A_{ij} is nonzero iff i==j, or i+m==j+n
   * The values increase with a counter from 1.0 in column major order.
   *
   * @pre A is a valid, allocated CSR matrix
   * @invariant A
   *
   * @param[in] A matrix::Csr* pointer to the matrix to be verified
   *
   * @return bool true if the matrix is valid, false otherwise
   */
  bool verifyCsrMatrix(matrix::Csr* A, real_type val = 0.0)
  {
    index_type* rowptr_csr = A->getRowData(memory::HOST);
    index_type* colidx_csr = A->getColData(memory::HOST);
    real_type* val_csr = A->getValues(memory::HOST);
    index_type n = A->getNumColumns();
    index_type m = A->getNumRows();

    if (n == m) {
        for (index_type i = 0; i < m; ++i) {
            if (i == m - 1) {
                if (rowptr_csr[i + 1] != rowptr_csr[i] + 1 ||
                    colidx_csr[rowptr_csr[i]] != n - 1 ||
                    val_csr[rowptr_csr[i]] != 2.0 * n + val) {
                    return false;
                }
            } else if (i == m / 2) {
                if (rowptr_csr[i + 1] != rowptr_csr[i] + 3 ||
                    colidx_csr[rowptr_csr[i]] != 0 ||
                    val_csr[rowptr_csr[i]] != 2.0 + val ||
                    colidx_csr[rowptr_csr[i] + 1] != n / 2 ||
                    colidx_csr[rowptr_csr[i] + 2] != n / 2 + 1 ||
                    val_csr[rowptr_csr[i] + 1] != 2.0 * (n / 2) + 2 + val ||
                    val_csr[rowptr_csr[i] + 2] != 2.0 * (n / 2) + 3 + val) {
                    return false;
                }
            } else {
                if (rowptr_csr[i + 1] != rowptr_csr[i] + 2 ||
                    colidx_csr[rowptr_csr[i]] != i ||
                    colidx_csr[rowptr_csr[i] + 1] != i + 1) {
                    return false;
                }
                if (i == 0) {
                    if (val_csr[rowptr_csr[i]] != 1.0 + val || val_csr[rowptr_csr[i] + 1] != 3.0 + val) {
                        return false;
                    }
                } else {
                    if (val_csr[rowptr_csr[i]] != 2.0 * (i + 1) + val ||
                        val_csr[rowptr_csr[i] + 1] != 2.0 * (i + 1) + 1.0 + val) {
                        return false;
                    }
                }
            }
        }
    } else if (n > m) {
        index_type main_diag_ind = 0;
        index_type off_diag_ind = n - m;
        real_type main_val = 1.0 + val;
        real_type off_val = n - m + 1.0 + val;
        for (index_type i = 0; i < m; ++i) {
            if (rowptr_csr[i + 1] != rowptr_csr[i] + 2 ||
                colidx_csr[rowptr_csr[i]] != main_diag_ind++ ||
                colidx_csr[rowptr_csr[i] + 1] != off_diag_ind++ ||
                val_csr[rowptr_csr[i]] != main_val++ ||
                val_csr[rowptr_csr[i] + 1] != off_val++) {
                return false;
            }
            if (i >= n - m - 1) main_val++;
            if (i < 2 * m - n) off_val++;
        }
    } else {
        real_type main_val = 1.0 + val;
        real_type off_val = 2.0 + val;
        for (index_type i = 0; i < m; ++i) {
            if (i < n && i < m - n) {
                if (rowptr_csr[i + 1] != rowptr_csr[i] + 1 ||
                    colidx_csr[rowptr_csr[i]] != i ||
                    val_csr[rowptr_csr[i]] != main_val) {
                    return false;
                }
                main_val += 2.0;
            } else if (i < n && i >= m - n) {
                if (rowptr_csr[i + 1] != rowptr_csr[i] + 2 ||
                    colidx_csr[rowptr_csr[i] + 1] != i ||
                    colidx_csr[rowptr_csr[i]] != i + n - m ||
                    val_csr[rowptr_csr[i] + 1] != main_val ||
                    val_csr[rowptr_csr[i]] != off_val) {
                    return false;
                }
                main_val += 2.0;
                off_val += 2.0;
            } else {
                if (rowptr_csr[i + 1] != rowptr_csr[i] + 1 ||
                    colidx_csr[rowptr_csr[i]] != i + n - m ||
                    val_csr[rowptr_csr[i]] != off_val) {
                    return false;
                }
                off_val += 2.0;
            }
        }
    }
    return true;
  }


  /**
   * @brief Create a CSR matrix with preset sparsity structure
   *
   * The sparisty structure is such that each row has a different number of nonzeros
   * The values are chosen so that the sum of each row is 30
   *
   * @param[in] n number of rows and columns
   *
   * @return matrix::Csr*
   */
  matrix::Csr* createCsrMatrix(const index_type n)
  {
    std::vector<real_type> r1 = {1., 5., 7., 8., 3., 2., 4.};
    std::vector<real_type> r2 = {1., 3., 2., 2., 1., 6., 7., 3., 2., 3.};
    std::vector<real_type> r3 = {11., 15., 4.};
    std::vector<real_type> r4 = {1., 1., 5., 1., 9., 2., 1., 2., 3., 2., 3.};
    std::vector<real_type> r5 = {6., 5., 7., 3., 2., 5., 2.};

    const std::vector<std::vector<real_type>> data = {r1, r2, r3, r4, r5};

    index_type nnz = 0;
    for (index_type i = 0; i < n; ++i) {
      size_t reminder = static_cast<size_t>(i % 5);
      nnz += static_cast<index_type>(data[reminder].size());
    }

    matrix::Csr* A = new matrix::Csr(n, n, nnz);
    A->allocateMatrixData(memory::HOST);

    index_type* rowptr = A->getRowData(memory::HOST);
    index_type* colidx = A->getColData(memory::HOST);
    real_type* val = A->getValues(memory::HOST);

    rowptr[0] = 0;
    for (index_type i = 0; i < n; ++i) {
      size_t reminder = static_cast<size_t>(i % 5);
      const std::vector<real_type>& row_sample = data[reminder];
      index_type nnz_per_row = static_cast<index_type>(row_sample.size());

      rowptr[i + 1] = rowptr[i] + nnz_per_row;
      for (index_type j = rowptr[i]; j < rowptr[i + 1]; ++j) {
        colidx[j] = (j - rowptr[i]) * n / nnz_per_row + (n % (n / nnz_per_row));
        val[j] = row_sample[static_cast<size_t>(j - rowptr[i])];
      }
    }
    A->setUpdated(memory::HOST);

    if (memspace_ == memory::DEVICE) {
      A->syncData(memspace_);
    }

    return A;
  }
}; // class MatrixHandlerTests

}} // namespace ReSolve::tests
