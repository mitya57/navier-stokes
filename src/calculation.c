#include "calculation.h"
#include "functions.h"

/* G and V values are packed as follows:
 *
 * G[0] G[1] ... G[M] V[1] V[2] ... V[M-1]
 *  0    1        M   M+1  M+2       2M-1
 *
 * These macros make it possible to easily change packing order in
 * the future.
 */
 
#define G_IND(i) (i)
#define V_IND(i) (grid->X_nodes - 1 + (i))

/* Useful for arrays construction */
#define NEW(type, len) ((type*)malloc((len)*sizeof(type)))

/* SparseMatrix operations */
#define APPEND(i, j, v) sparse_matrix_append(&matrix, i, j, v)
#define APPEND_C(i, j, v, cond) if (cond) APPEND(i, j, v)

void next_TimeLayer_Calculate (
    double * G, 
    double * V, 
    Node_status * node_status, 
    double * space_coordinates, 
    Gas_parameters * parameters,
    Grid * grid) {
  QMatrix lh_side; // left-hand side of the system
  Vector rh_side; // right-hand side of the system
  Vector unknown_vector; // which will be found when we solve the system
  unsigned time_step, i;

  Q_Constr (&lh_side, 
            "Matrix", 
            2 * grid->X_nodes, // side
            False, // non-symmetric
            Rowws, // row-wise storage
            Normal, // internal laspack stuff
            True); // internal laspack stuff
  V_Constr (&rh_side, 
            "Right-hand side of equation", 
            2 * grid->X_nodes, // size
            Normal, // internal laspack stuff
            True); // internal laspack stuff
  V_Constr (&unknown_vector, 
            "Unknown vector", 
            2 * grid->X_nodes, 
            Normal, 
            True);
  SetRTCAccuracy (1e-8);

  // T == 0
  fill_mesh_at_initial_time (G, V, log_ro, u_exact, space_coordinates, grid->X_nodes); 

  for (time_step = 1; time_step < grid->T_nodes; ++time_step) {
    fill_system (&lh_side, &rh_side, grid, node_status, parameters, G, V);

    // launch iteration algorithm
    CGNIter (&lh_side, 
            &unknown_vector, 
            &rh_side, 
            2000, // max iterations
            JacobiPrecond, // preconditioner type
            1.2); // preconditioner relaxation constant; probably, should be changed

    for (i = 0; i < grid->X_nodes; ++i) {
      G[i] = V_GetCmp (&unknown_vector, G_IND(i));
    }
    for (i = 1; i < grid->X_nodes - 1; ++i) {
      V[i] = V_GetCmp (&unknown_vector, V_IND(i));
    }
  }

  Q_Destr (&lh_side);
  V_Destr (&unknown_vector);
  V_Destr (&rh_side);

  return;
}

void sparse_matrix_clear (
    SparseMatrix * this) {

  unsigned i;
  for (i = 0; i < this->rows; ++i) {
    free(this->cols_numbers[i]);
    free(this->values[i]);
  }
  free(this->cols);
  free(this->rows_numbers);
  free(this->cols_numbers);
  free(this->values);
}

void sparse_matrix_append (
    SparseMatrix * this,
    unsigned row,
    unsigned col,
    double value) {

  unsigned i;
  for (i = 0; i < this->rows; ++i) {
    if (this->rows_numbers[i] == row) {
      this->cols_numbers[i][this->cols[i]] = col;
      this->values[i][this->cols[i]] = value;
      ++(this->cols[i]);
      return;
    }
  }

  this->rows_numbers[this->rows] = row;
  this->cols[this->rows] = 1;
  this->cols_numbers[this->rows][0] = col;
  this->values[this->rows][0] = value;
  ++(this->rows);
}

void sparse_matrix_write (
    SparseMatrix *this,
    QMatrix *qmatrix) {

  /* This assumes `qmatrix' is empty when this is called. */

  unsigned i, j;
  for (i = 0; i < this->rows; ++i) {
    Q_SetLen(qmatrix, this->rows_numbers[i], this->cols[i]);
    for (j = 0; j < this->cols[i]; ++j) {
      Q_SetEntry(qmatrix, this->rows_numbers[i], j, this->cols_numbers[i][j], this->values[i][j]);
    }
  }
}

void sparse_matrix_initialize (
    SparseMatrix * this,
    unsigned rows_default,
    unsigned cols_default) {

  unsigned i;
  this->rows = 0;
  this->cols = NEW(unsigned, rows_default);
  this->rows_numbers = NEW(unsigned, rows_default);
  this->cols_numbers = NEW(unsigned*, rows_default);
  this->values = NEW(double*, rows_default);
  for (i = 0; i < rows_default; ++i) {
    this->cols_numbers[i] = NEW(unsigned, cols_default);
    this->values[i] = NEW(double, rows_default);
  }
}
/*
void fill_system (
    QMatrix * lh_side,
    Vector * rh_side,
    Grid * grid,
    Node_status * node_status,
    Gas_parameters * parameters,
    double * G,
    double * V) {

  unsigned m, M = grid->X_nodes - 1;
  double h = grid->X_step, tau = grid->T_step;

  SparseMatrix matrix;
  sparse_matrix_initialize(&matrix, 2 * M, 5);

  APPEND(G_IND(0), G_IND(0), 1. / tau - .5 * V[0] / h);
  APPEND(G_IND(0), G_IND(1), .5 * V[1] / h);
  APPEND(G_IND(0), V_IND(0), -1. / h);
  APPEND(G_IND(0), V_IND(1), 1. / h);
  V_SetCmp(rh_side, G_IND(0), G[0] / tau + G[0] * .5 * (V[1] - V[0]) / h +
                              .25 * (G[2] * V[2] - 2 * G[1] * V[1] + G[0] * V[0]) / h +
                              .25 * (V[2] - 2 * V[1] + V[0]) * (2 - G[0]) / h);

  for (m = 1; m < M; ++m) {
    APPEND(G_IND(m), G_IND(m - 1), -(V[m] + V[m-1]) * .25 / h);
    APPEND(G_IND(m), G_IND(m), G[m] / tau);
    APPEND(G_IND(m), G_IND(m + 1), (V[m] + V[m+1]) * .25 / h);
    APPEND(G_IND(m), V_IND(m - 1), -.5 / h);
    APPEND(G_IND(m), V_IND(m + 1), .5 / h);
    V_SetCmp(rh_side, G_IND(m), G[m] / tau + G[m] * (V[m+1] - V[m-1]) * .25 / h);
  }

  APPEND(G_IND(M), G_IND(M - 1), -.5 * V[M-1] / h);
  APPEND(G_IND(M), G_IND(M), 1. / tau + .5 * V[M] / h);
  APPEND(G_IND(M), V_IND(M - 1), -1. / h);
  APPEND(G_IND(M), V_IND(M), 1. / h);

  V_SetCmp(rh_side, G_IND(M), G[M] / tau + G[M] * .5 * (V[M] - V[M-1]) / h -
                              .25 * (G[M] * V[M] - 2 * G[M-1] * V[M-1] + G[M-2] * V[M-2]) / h -
                              .25 * (V[M] - 2 * V[M-1] + V[M-2]) * (2 - G[M]) / h);

  for (m = 1; m < M; ++m) {
    APPEND  (V_IND(m), G_IND(m - 1), -.5 * parameters->p_ro / h);
    APPEND  (V_IND(m), G_IND(m + 1), .5 * parameters->p_ro / h);
    APPEND_C(V_IND(m), V_IND(m - 1), -(V[m] + V[m-1]) / (h * 6), m > 1);
    APPEND  (V_IND(m), V_IND(m), 1. / tau);
    APPEND_C(V_IND(m), V_IND(m + 1), (V[m] + V[m+1]) / (h * 6), m < M - 1);
    V_SetCmp(rh_side, V_IND(m), V[m] / tau);
  }
  sparse_matrix_write(&matrix, lh_side);
  sparse_matrix_clear(&matrix);
}
*/

void set_qmatrix_entries (
    QMatrix * matrix, 
    unsigned row, 
    unsigned * nonzero_columns, 
    double * values, 
    unsigned total_entries) {
  unsigned entry_number;
  for (entry_number = 0; entry_number < total_entries; ++ entry_number) {
    Q_SetEntry (matrix, row, entry_number, nonzero_columns[entry_number], values[entry_number]);
  }
  return;
}

void fill_system (
    QMatrix * lh_side,
    Vector * rh_side,
    Grid * grid,
    Node_status * node_status,
    Gas_parameters * parameters,
    double * G,
    double * V) {
  unsigned space_step = 0;
  unsigned first_type_equation_coef_length = 5;
  unsigned second_type_equation_coef_length = 4;
  unsigned third_type_equation_coef_length = 4;
  unsigned fourth_type_equation_coef_length = 5;
  unsigned equation_number = 0;

  unsigned nonzero_columns[5];
  double lh_values[5];
  double rh_value;

  for (space_step = 0; space_step < grid->X_nodes; ++space_step) {
    switch (node_status[space_step]) {
      case LEFT:
        nonzero_columns[0] = 1; // G_0
        nonzero_columns[1] = 2; // V_0
        nonzero_columns[2] = 3; // G_1
        nonzero_columns[3] = 4; // V_1

        lh_values[0] = grid->T_step - .5 * grid->X_step * V[0];
        lh_values[1] = -grid->X_step;
        lh_values[2] = .5 * grid->X_step * V[1];
        lh_values[3] = grid->X_step;

        rh_value = 0.;

        Q_SetLen (lh_side, equation_number, second_type_equation_coef_length);

        set_qmatrix_entries (lh_side, equation_number, nonzero_columns, lh_values, second_type_equation_coef_length);

        V_SetCmp (rh_side, equation_number, rh_value);
        ++equation_number;
        break;

      case MIDDLE:
        nonzero_columns[0] = 2 * space_step - 1; // G_{n-1}
        nonzero_columns[1] = 2 * space_step;     // V_{n-1}
        nonzero_columns[2] = 2 * space_step + 1; // G_{n}
        nonzero_columns[3] = 2 * space_step + 3; // G_{n+1}
        nonzero_columns[4] = 2 * space_step + 4; // V_{n+1}

        lh_values[0] = -.25 * grid->X_step * V[space_step] - .25 * grid->X_step * V[space_step - 1];
        lh_values[1] = -grid->X_step;
        lh_values[2] = grid->T_step;
        lh_values[3] = .25 * grid->X_step * V[space_step] + .25 * grid->X_step * V[space_step + 1];
        lh_values[4] = grid->X_step;

        rh_value = 0.;

        Q_SetLen (lh_side, equation_number, first_type_equation_coef_length);

        set_qmatrix_entries (lh_side, equation_number, nonzero_columns, lh_values, first_type_equation_coef_length);

        V_SetCmp (rh_side, equation_number, rh_value);
        ++equation_number;

        /* another equation */

        nonzero_columns[0] = 2 * space_step - 1; // G_{n-1}
        nonzero_columns[1] = 2 * space_step;     // V_{n-1}
        nonzero_columns[2] = 2 * space_step + 2; // V_{n}
        nonzero_columns[3] = 2 * space_step + 3; // G_{n+1}
        nonzero_columns[4] = 2 * space_step + 4; // V_{n+1}

        // attention: these values should be changed when (viscosity != 0)
        lh_values[0] = -.5 * grid->X_step * parameters->p_ro;
        lh_values[1] = -1. / 6 * grid->X_step * V[space_step] - 
                        1. / 6 * grid->X_step * V[space_step - 1];
        lh_values[2] = grid->T_step;
        lh_values[3] = .5 * grid->X_step * parameters->p_ro;
        lh_values[4] = 1. / 6 * grid->X_step * V[space_step] + 
                       1. / 6 * grid->X_step * V[space_step + 1];

        rh_value = 0.;

        Q_SetLen (lh_side, equation_number, fourth_type_equation_coef_length);

        set_qmatrix_entries (lh_side, equation_number, nonzero_columns, lh_values, fourth_type_equation_coef_length);

        V_SetCmp (rh_side, equation_number, rh_value);
        ++equation_number;
        break;

      case RIGHT:
        nonzero_columns[0] = grid->X_nodes - 3; // G_{M-1}
        nonzero_columns[1] = grid->X_nodes - 2; // V_{M-1}
        nonzero_columns[2] = grid->X_nodes - 1; // G_{M}
        nonzero_columns[3] = grid->X_nodes;     // V_{M}

        lh_values[0] = -.5 * grid->X_step * V[grid->X_nodes - 2];
        lh_values[1] = -grid->X_step;
        lh_values[2] = grid->T_step + .5 * grid->X_step * V[grid->X_nodes - 1];
        lh_values[3] = grid->X_step;

        rh_value = 0.;

        Q_SetLen (lh_side, equation_number, third_type_equation_coef_length);

        set_qmatrix_entries (lh_side, equation_number, nonzero_columns, lh_values, third_type_equation_coef_length);

        V_SetCmp (rh_side, equation_number, rh_value);
        ++equation_number;
        break;

      default:
        break;
    }
  }
}

void fill_mesh_at_initial_time (
    double * G,
    double * V,
    double (*g) (double, double), // log (ro_0)
    double (*v) (double, double), // u_0
    double * space_coordinates,
    unsigned space_nodes) {
  unsigned space_step = 0;
  for (space_step = 0; space_step < space_nodes; ++space_step) {
    G[space_step] = g(space_coordinates[space_step], 0);
    V[space_step] = v(space_coordinates[space_step], 0);
  }
  return;
}
