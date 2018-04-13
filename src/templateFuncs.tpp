//
// Created by ward_s on 15/03/18.
//

#include <armadillo>


//template<typename T>
//T asEvaled(const T &X) {
//    return X.eval();
//}

template<typename T>
arma::Mat<typename T::elem_type>armaModM(const T &X, int n) {
    return X - n*arma::floor(X/n);
}

template<typename T>
arma::Cube<typename T::elem_type>armaModC(const T &X, int n) {
    return X - n*arma::floor(X/n);
}

template<typename T>
static arma::Cube<typename T::elem_type> permute(const T &cube, int order[]) {
    using namespace arma;

    int op1 = order[0];
    int op2 = order[1];
    int op3 = order[2];

    int perm = op1 * 100 + op2 * 10 + op3;

    if (perm == 123) {
        return cube;
    }

    uword dimension[] = {cube.n_rows, cube.n_cols, cube.n_slices};

    uword rows = dimension[op1 - 1];
    uword cols = dimension[op2 - 1];
    uword slis = dimension[op3 - 1];


    Cube<typename T::elem_type> output(rows, cols, slis, fill::zeros);

#pragma omp parallel for
    for (int s = 0; s < cube.n_slices; ++s) {
        auto this_slice = cube.slice(s); // rxc

        if (op1 == 2 & op2 == 1) {
            if (op3 == 3) { // 2 1 3
                output.slice(s) = this_slice.t(); // cxr = (rxc)'
            } else {
                throw std::invalid_argument(std::string("Element is duplicated"));
            }
        } else { // [1 2 3] [1 2 3] [1 2 3]
            if (op3 == 1) { // [1 2 3] [1 2 3] [1]
                for (int r = 0; r < cube.n_rows; ++r) {
                    if (op1 == 2) { // [2] [1 2 3] [1]
                        output.slice(r)(arma::span::all, s) = vectorise(this_slice(r, arma::span::all));
                    } else if (op1 == 3) { // [3] [1 2 3] [1]
                        output.slice(r)(s, arma::span::all) = this_slice(r, arma::span::all);
                    } else {
                        throw std::invalid_argument(std::string("Element is duplicated"));
                    }
                }
            } else if (op3 == 2) { // [1 2 3] [1 2 3] [2]
                for (int c = 0; c < cube.n_cols; ++c) {
                    if (op2 == 1) { // [1 2 3] [1] [2]
                        output.slice(c)(s, arma::span::all) = this_slice(arma::span::all, c).t();
                    } else if (op2 == 3) { // [1 2 3] [3] [2]
                        output.slice(c)(arma::span::all, s) = this_slice(arma::span::all, c);
                    } else {
                        throw std::invalid_argument(std::string("Element is duplicated"));
                    }
                }
            } else{
                throw std::invalid_argument(std::string("Element is duplicated"));
            }
        }
    }

    return output;
}

template<typename T, typename TT>
arma::Mat<typename T::elem_type> mmat(const T &X, const TT &Y, int dim[]) {

    arma::Mat <typename T::elem_type> YY(Y);

    arma::uword nA[2];
    arma::uword nB[2];

    if (dim[0] == dim[1]){
        throw std::invalid_argument(std::string("Dimension element is duplicated"));
    }
    if ((dim[0] > 2) || (dim[0] < 0) || (dim[1] > 2) || (dim[1] < 0)){
        throw std::invalid_argument(std::string("Dimension element has to be 1 or 2"));
    }

    if ((dim[0] == 2) & (dim[1] == 1)){
        nA[0] = X.n_cols; nA[1] = X.n_rows;
        nB[0] = YY.n_cols; nB[1] = YY.n_rows;
    } else{
        nA[0] = X.n_rows; nA[1] = X.n_cols;
        nB[0] = YY.n_rows; nB[1] = YY.n_cols;
    }
    arma::uword repv[3] = {1, 1, 1};
    repv[dim[0]-1] = nA[0];

    int idx[3];
    idx[(dim[0]+1)%2] = 3;
    idx[dim[0]%2]     = dim[0];
    idx[2]            = dim[1];

    // Create temp and return type.
    arma::Cube<typename T::elem_type>A(X.n_rows, X.n_cols, nB[1], arma::fill::zeros);
    arma::Cube<typename T::elem_type>B;
    arma::Mat <typename T::elem_type>C;

    // Build A
    A.each_slice([X](arma::Mat<typename T::elem_type>&this_slice){this_slice = X;});

    // Build B
    if (idx[1] == 1)
    {
        B = arma::zeros(repv[0], YY.n_rows, YY.n_cols);
        for (arma::uword i = 0; i < B.n_slices; i++) {
            B.slice(i) = arma::repmat(YY.col(i).t(), repv[0], repv[1]);
        }
    } else {
        B = arma::zeros(YY.n_cols, repv[1], YY.n_rows);
        for (arma::uword i = 0; i < B.n_slices; i++) {
            B.slice(i) = arma::repmat(YY.row(i).t(), repv[0], repv[1]);
        }
    }

    // Inplace element multiplication A*B
    A%=B;

    // Create return matrix C
    if (dim[0] == 1) {
        // We sum along cols...
        C = arma::zeros(repv[0], A.n_slices);
        for (arma::uword i = 0; i < A.n_cols; i++){
            C += A(arma::span::all, arma::span(i,i), arma::span::all);
        }
    }
    else {
        // We sum along rows
        C = arma::zeros(A.n_slices, repv[1]);
        for (arma::uword i = 0; i < A.n_rows; i++){
            C += A(arma::span(i,i), arma::span::all, arma::span::all);
        }
    }
    return C;
}

template <typename T> arma::Mat<typename T::elem_type> cmod(T &inMat){
    arma::Mat<typename T::elem_type> retMat = armaModM(inMat,1);
    retMat = arma::find(retMat > (arma::ones(retMat.n_rows,retMat.n_cols) - retMat)) -= 1;
    return retMat;
}

template <typename T> arma::Mat<typename T::elem_type> uniquetol(T inMat, double tol){
/*
    Returns unique column vectors within the given
    `tol` tolerance. Two column vectors are considered
    unequal, if the distance between them is larger than
    the tolerance ($\delta$):
*/
    arma::Mat<typename T::elem_type> unique; // Placeholder for the return

    while (inMat.n_cols > 0) {
        // Create keep/destroy vectors
        std::vector<arma::uword> keepIDX, destIDX;
        keepIDX.reserve(inMat.n_cols);
        destIDX.reserve(inMat.n_cols);

        // Loop over and check if we want to keep or destroy
        for (arma::uword ind = 0; ind < inMat.n_cols; ind++) {
            if (!arma::approx_equal(inMat(arma::span(), 0), inMat(arma::span(), ind), "absdiff", tol)) {
                keepIDX.push_back(ind);
            } else {
                destIDX.push_back(ind);
            }
        }
        // Copy first to unique
        unique = arma::join_rows(unique, inMat(arma::span(), destIDX[0]));
        // Purge all duplicates (including 0 idx)
        inMat= inMat.cols(arma::uvec(keepIDX));
    }
    return unique;
}
